#include "GameSystem.h"
#include "../project/SceneTemplates.h"
#include "../physics/PhysicsSystem.h"
#include "../core/Window.h"
#include "../core/Log.h"
#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace Hargon
{
    static constexpr float kTwoPi = 6.28318530718f;

    void GameSystem::StepAnimationsAndParticles(
        Scene& scene,
        Renderer3D& renderer,
        float dt)
    {
        for (auto& e : scene.GetEntities())
        {
            if (!scene.ShouldProcessEntity(e, false))
                continue;

            // 1. Godot AnimationPlayer Track Evaluation
            if (e.animation.enabled && e.animation.playing)
            {
                if (!e.animation.originCaptured)
                {
                    e.animation.originPos = e.transform.position;
                    e.animation.originRot = e.transform.rotation;
                    e.animation.originScale = e.transform.scale;
                    e.animation.originCaptured = true;
                }

                e.animation.currentTime += dt * e.animation.speed;
                float t = e.animation.currentTime;
                float amp = e.animation.amplitude;

                switch (e.animation.trackType)
                {
                case 0: // IdleBob + gentle Y spin
                    e.transform.position.y = e.animation.originPos.y + std::sin(t * 2.2f) * amp;
                    e.transform.rotation.y += dt * 42.0f;
                    break;
                case 1: // SpinContinuous around axis
                {
                    glm::vec3 ax = glm::length(e.animation.axis) > 0.001f ? glm::normalize(e.animation.axis) : glm::vec3(0.0f, 1.0f, 0.0f);
                    e.transform.rotation += ax * (e.animation.speed * dt);
                    break;
                }
                case 2: // PatrolPingPong along axis
                {
                    glm::vec3 ax = glm::length(e.animation.axis) > 0.001f ? glm::normalize(e.animation.axis) : glm::vec3(1.0f, 0.0f, 0.0f);
                    e.transform.position = e.animation.originPos + ax * (std::sin(t) * amp * 4.0f);
                    break;
                }
                case 3: // PulseScale
                {
                    float s = 1.0f + std::sin(t * 3.0f) * (amp * 0.35f);
                    e.transform.scale = e.animation.originScale * s;
                    break;
                }
                case 4: // OrbitCircle in XZ
                    e.transform.position.x = e.animation.originPos.x + std::cos(t) * amp * 3.0f;
                    e.transform.position.z = e.animation.originPos.z + std::sin(t) * amp * 3.0f;
                    break;
                case 5: // PendulumSwing
                    e.transform.rotation.z = e.animation.originRot.z + std::sin(t * 2.0f) * (amp * 35.0f);
                    break;
                }
            }

            // 2. Godot CPUParticles3D Continuous Emitter Evaluation
            if (e.particleEmitter.enabled && e.particleEmitter.emitting && e.particleEmitter.emissionRate > 0.0f)
            {
                e.particleEmitter.spawnAccumulator += e.particleEmitter.emissionRate * dt;
                int count = static_cast<int>(e.particleEmitter.spawnAccumulator);
                if (count > 0)
                {
                    e.particleEmitter.spawnAccumulator -= static_cast<float>(count);
                    count = std::min(count, 8);
                    glm::vec3 worldPos = scene.GetWorldPosition(e);
                    for (int i = 0; i < count; ++i)
                    {
                        float phase = static_cast<float>((e.id * 17 + i * 31) % 100) / 100.0f * kTwoPi + e.animation.currentTime;
                        glm::vec3 offset(0.0f);
                        if (e.particleEmitter.emissionShape == 1) // Sphere
                        {
                            offset = glm::vec3(std::cos(phase), std::sin(phase * 1.7f), std::sin(phase)) * (e.particleEmitter.emissionRadius * 0.5f);
                        }
                        else if (e.particleEmitter.emissionShape == 2) // Box
                        {
                            offset = glm::vec3(std::cos(phase) * e.particleEmitter.emissionBoxExtents.x,
                                               0.2f,
                                               std::sin(phase) * e.particleEmitter.emissionBoxExtents.z);
                        }
                        else if (e.particleEmitter.emissionShape == 3) // Ring
                        {
                            offset = glm::vec3(std::cos(phase) * e.particleEmitter.emissionRadius,
                                               0.25f,
                                               std::sin(phase) * e.particleEmitter.emissionRadius);
                        }

                        glm::vec3 col = glm::mix(e.particleEmitter.startColor, e.particleEmitter.endColor, (i % 2) ? 0.25f : 0.75f);
                        renderer.SpawnParticleBurst(worldPos + offset, col, 1, e.particleEmitter.initialSpeed);
                    }
                }
            }
        }
    }

    void GameSystem::StepGameplay(
        Scene& scene,
        PhysicsSystem& physics,
        ScriptEngine& scripts,
        AudioEngine& audio,
        Renderer3D& renderer,
        Window* window,
        float dt)
    {
        if (window && window->IsKeyPressedByName("R") && (scene.gameOver || scene.gameWon))
        {
            scene.restartRequested = true;
            audio.PlaySound2D("wave_start", 0.8f, 1.0f);
            return;
        }

        if (scene.hudBannerTimer > 0.0f)
            scene.hudBannerTimer = std::max(0.0f, scene.hudBannerTimer - dt);

        StepAnimationsAndParticles(scene, renderer, dt);

        if (scene.gameOver || scene.gameWon)
        {
            renderer.UpdateParticles(dt);
            return;
        }

        // 1. Update Lua Scripts
        scripts.UpdateScripts(scene, dt);

        // 2. Step Physics & Collisions
        physics.Step(scene, dt);

        // 3. Process Projectile Lifetimes, Shield Regen & Listener Pose via Tags
        Entity* activePlayer = nullptr;
        {
            auto players = scene.FindEntitiesByTag("Player");
            if (!players.empty())
                activePlayer = scene.GetEntity(players[0]);
        }

        for (auto& e : scene.GetEntities())
        {
            if (e.alive && e.audioSource.enabled && e.audioSource.playOnAwake && !e.audioSource.awakePlayed && scene.ShouldProcessEntity(e, false))
            {
                e.audioSource.awakePlayed = true;
                glm::vec3 wp = scene.GetWorldPosition(e);
                if (e.audioSource.spatial)
                {
                    audio.PlaySound3D(
                        e.audioSource.soundName, wp,
                        e.audioSource.volume, e.audioSource.pitch,
                        e.audioSource.attenuationModel, e.audioSource.unitSize,
                        e.audioSource.maxDistance, e.audioSource.pitchRandomness);
                }
                else
                {
                    audio.PlaySound2D(e.audioSource.soundName, e.audioSource.volume, e.audioSource.pitch);
                }
            }

            if (!scene.ShouldProcessEntity(e, false) || !e.gameplay.enabled)
                continue;

            if (e.gameplay.maxLifetime > 0.0f)
            {
                e.gameplay.lifetime += dt;
                if (e.gameplay.lifetime >= e.gameplay.maxLifetime)
                {
                    e.alive = false;
                }
            }
            else if (e.tag.tag == "Player")
            {
                e.gameplay.shield = std::min(e.gameplay.maxShield, e.gameplay.shield + 5.0f * dt);
                audio.SetListenerTransform(scene.GetWorldPosition(e), e.transform.GetForward());
            }
        }

        // 4. Process Trigger Enter/Exit Signals + Collision Events
        for (const auto& ev : physics.GetTriggerEntered())
        {
            scene.EmitSignal("trigger_entered", ev.entityA, "entered");
            scripts.DispatchTrigger(scene, ev.entityA, ev.entityB, true);
        }
        for (const auto& ev : physics.GetTriggerExited())
        {
            scene.EmitSignal("trigger_exited", ev.entityA, "exited");
            scripts.DispatchTrigger(scene, ev.entityA, ev.entityB, false);
        }

        for (const auto& col : physics.GetCollisions())
        {
            scripts.DispatchCollision(scene, col.entityA, col.entityB);

            Entity* a = scene.GetEntity(col.entityA);
            Entity* b = scene.GetEntity(col.entityB);
            if (!a || !b || !a->alive || !b->alive)
                continue;

            auto matchPair = [&](const std::string& tag1, const std::string& tag2, Entity*& out1, Entity*& out2) -> bool {
                if (a->tag.tag == tag1 && b->tag.tag == tag2) { out1 = a; out2 = b; return true; }
                if (b->tag.tag == tag1 && a->tag.tag == tag2) { out1 = b; out2 = a; return true; }
                return false;
            };

            Entity* p1 = nullptr;
            Entity* p2 = nullptr;

            // Player/Turret Projectile hits Enemy
            if (matchPair("Projectile", "Enemy", p1, p2))
            {
                p2->gameplay.health -= p1->gameplay.damage;
                p1->alive = false;
                renderer.SpawnParticleBurst(col.contactPoint, glm::vec3(0.25f, 0.92f, 1.0f), 10, 7.5f);
                audio.PlaySound3D("hit", col.contactPoint, 0.65f, 1.15f);

                if (p2->gameplay.health <= 0.0f)
                {
                    glm::vec3 deathPos = p2->transform.position;
                    p2->alive = false;
                    scene.score += p2->gameplay.scoreValue;
                    ++scene.enemiesKilled;
                    renderer.SpawnParticleBurst(deathPos, glm::vec3(1.0f, 0.45f, 0.15f), 28, 12.5f);
                    audio.PlaySound3D("explosion", deathPos, 0.9f, 1.0f);
                    scene.EmitSignal("enemy_slain", p2->id, p2->tag.name);

                    if ((scene.enemiesKilled % 2) == 0)
                    {
                        SceneTemplates::SpawnEnergyCrystal(scene, deathPos, (scene.enemiesKilled % 4) == 0);
                    }
                }
            }
            // Enemy Projectile hits Player
            else if (matchPair("EnemyProjectile", "Player", p1, p2))
            {
                float dmg = p1->gameplay.damage;
                p1->alive = false;
                if (p2->gameplay.shield >= dmg)
                {
                    p2->gameplay.shield -= dmg;
                }
                else
                {
                    float rem = dmg - p2->gameplay.shield;
                    p2->gameplay.shield = 0.0f;
                    p2->gameplay.health = std::max(0.0f, p2->gameplay.health - rem);
                }
                renderer.SpawnParticleBurst(col.contactPoint, glm::vec3(1.0f, 0.25f, 0.20f), 12, 8.0f);
                audio.PlaySound2D("hit", 0.75f, 0.85f);
            }
            // Enemy Projectile hits Core
            else if (matchPair("EnemyProjectile", "Core", p1, p2))
            {
                p2->gameplay.health = std::max(0.0f, p2->gameplay.health - p1->gameplay.damage);
                p1->alive = false;
                renderer.SpawnParticleBurst(col.contactPoint, glm::vec3(1.0f, 0.35f, 0.15f), 12, 8.0f);
                audio.PlaySound3D("hit", col.contactPoint, 0.75f, 0.9f);
            }
            // Player collects Crystal / Beacon Pickup
            else if (matchPair("Player", "Crystal", p1, p2))
            {
                p1->gameplay.health = std::min(p1->gameplay.maxHealth, p1->gameplay.health + p2->gameplay.health);
                p1->gameplay.shield = std::min(p1->gameplay.maxShield, p1->gameplay.shield + 25.0f);
                scene.score += p2->gameplay.scoreValue;
                ++scene.crystalsCollected;
                renderer.SpawnParticleBurst(p2->transform.position, glm::vec3(0.3f, 0.98f, 1.0f), 18, 9.0f);
                audio.PlaySound2D("pickup", 0.85f, 1.0f);
                scene.EmitSignal("item_collected", p2->id, p2->tag.name);
                p2->alive = false;
            }
            // Enemy or Obstacle rams Player / Core
            else if (matchPair("Enemy", "Player", p1, p2) || matchPair("Enemy", "Core", p1, p2))
            {
                p2->gameplay.health = std::max(0.0f, p2->gameplay.health - 8.0f * dt);
                glm::vec3 pushDir = p1->transform.position - p2->transform.position;
                pushDir.y = 0.0f;
                if (glm::length(pushDir) > 0.01f)
                    p1->rigidBody.velocity = glm::normalize(pushDir) * 10.0f;
            }
            // Projectiles hitting obstacles
            else if (matchPair("Projectile", "Obstacle", p1, p2) || matchPair("EnemyProjectile", "Obstacle", p1, p2))
            {
                p1->alive = false;
                renderer.SpawnParticleBurst(col.contactPoint, glm::vec3(0.7f, 0.8f, 1.0f), 6, 5.0f);
            }
        }

        scene.FlushDestroyedEntities();
        renderer.UpdateParticles(dt);

        // 5. Check Player / Core Survival & Wave / Objective Progression (Pure Tag-Based)
        Entity* core = nullptr;
        {
            auto cores = scene.FindEntitiesByTag("Core");
            if (!cores.empty())
                core = scene.GetEntity(cores[0]);
        }

        if ((activePlayer && activePlayer->gameplay.health <= 0.0f) || (core && core->gameplay.health <= 0.0f))
        {
            scene.gameOver = true;
            scene.hudBannerText = scene.defeatText.empty() ? "MISSION FAILED! PRESS [R] TO RESTART" : scene.defeatText;
            scene.hudBannerTimer = 999.0f;
            audio.PlaySound2D("explosion", 1.0f, 0.7f);
            return;
        }

        scene.enemiesRemainingInWave = static_cast<int>(scene.FindEntitiesByTag("Enemy").size());
        int collectiblesLeft = static_cast<int>(scene.FindEntitiesByTag("Crystal").size());
        bool hadAnyObjectives = (scene.maxWaves > 1 || scene.enemiesKilled > 0 || scene.crystalsCollected > 0);

        if (hadAnyObjectives && scene.enemiesRemainingInWave == 0 && (scene.maxWaves > 1 || collectiblesLeft == 0))
        {
            if (scene.wave < scene.maxWaves)
            {
                scene.waveCooldownTimer += dt;
                if (scene.waveCooldownTimer >= 1.8f)
                {
                    scene.waveCooldownTimer = 0.0f;
                    ++scene.wave;
                    int spawnCount = 4 + scene.wave * 2;
                    for (int i = 0; i < spawnCount; ++i)
                    {
                        float angle = (static_cast<float>(i) / spawnCount) * kTwoPi;
                        float radius = 26.0f + (i % 2) * 4.0f;
                        glm::vec3 pos(std::cos(angle) * radius, 0.9f, std::sin(angle) * radius);
                        SceneTemplates::SpawnEnemyDrone(scene, pos, scene.wave);
                    }
                    SceneTemplates::SpawnEnergyCrystal(scene, glm::vec3(-10.0f, 0.95f, 10.0f), true);
                    SceneTemplates::SpawnEnergyCrystal(scene, glm::vec3( 10.0f, 0.95f, 10.0f), false);

                    scene.enemiesRemainingInWave = spawnCount;
                    scene.hudBannerText = "WAVE " + std::to_string(scene.wave) + " INCOMING: " + std::to_string(spawnCount) + " HOSTILES!";
                    scene.hudBannerTimer = 3.5f;
                    audio.PlaySound2D("wave_start", 0.9f, 1.0f);
                }
            }
            else
            {
                scene.gameWon = true;
                scene.score += 1000;
                scene.hudBannerText = scene.victoryText.empty() ? "VICTORY!" : scene.victoryText;
                scene.hudBannerTimer = 999.0f;
                audio.PlaySound2D("victory", 0.95f, 1.0f);
                if (core)
                    renderer.SpawnParticleBurst(scene.GetWorldPosition(*core) + glm::vec3(0, 2.5f, 0), glm::vec3(0.3f, 1.0f, 0.6f), 45, 14.0f);
            }
        }
    }

    void GameSystem::UpdateGameCamera(const Scene& scene, CameraView& ioCamera, float dt)
    {
        const Entity* camEntity = nullptr;
        for (const auto& e : scene.GetEntities())
        {
            if (e.alive && e.camera.enabled && e.camera.isPrimary)
            {
                camEntity = &e;
                break;
            }
        }

        glm::vec3 followOffset(0.0f, 13.5f, -14.5f);
        glm::vec3 lookOffset(0.0f, 0.5f, 3.8f);
        float smooth = 8.0f;
        std::string targetName;
        CameraMode mode = CameraMode::FollowOffset;
        float armLen = 13.0f;
        float pitchDeg = 22.0f;
        float yawDeg = 0.0f;

        if (camEntity)
        {
            ioCamera.fovDegrees = camEntity->camera.fov;
            ioCamera.nearClip = camEntity->camera.nearClip;
            ioCamera.farClip = camEntity->camera.farClip;
            mode = camEntity->camera.mode;
            followOffset = camEntity->camera.followOffset;
            lookOffset = camEntity->camera.followLookOffset;
            smooth = camEntity->camera.smoothSpeed;
            armLen = camEntity->camera.springArmLength;
            pitchDeg = camEntity->camera.orbitPitch;
            yawDeg = camEntity->camera.orbitYaw;
            targetName = camEntity->camera.followTargetName;

            if (mode == CameraMode::FixedStatic)
            {
                ioCamera.position = scene.GetWorldPosition(*camEntity);
                ioCamera.forward = camEntity->transform.GetForward();
                return;
            }
        }

        const Entity* target = targetName.empty() ? nullptr : const_cast<Scene&>(scene).FindEntityByName(targetName);
        if (!target)
        {
            auto players = scene.FindEntitiesByTag("Player");
            if (!players.empty())
                target = scene.GetEntity(players[0]);
        }

        if (target)
        {
            glm::vec3 targetPos = scene.GetWorldPosition(*target);
            glm::vec3 desiredPos = targetPos + followOffset;
            glm::vec3 lookTarget = targetPos + lookOffset;

            if (mode == CameraMode::ThirdPersonOrbit)
            {
                float pr = glm::radians(pitchDeg);
                float yr = glm::radians(yawDeg);
                glm::vec3 armDir(
                    -std::sin(yr) * std::cos(pr),
                     std::sin(pr),
                    -std::cos(yr) * std::cos(pr));
                lookTarget = targetPos + lookOffset;
                float effectiveArm = armLen;
                // Godot SpringArm3D-inspired collision pull-in against world geometry
                PhysicsRaycastHit hit = PhysicsSystem::Raycast(
                    scene, lookTarget, armDir, armLen, 0xFFFFFFFFu, target->id, "Projectile");
                if (hit.hit)
                    effectiveArm = std::clamp(hit.distance - 0.55f, 1.8f, armLen);
                desiredPos = lookTarget + armDir * effectiveArm;
            }
            else if (mode == CameraMode::FirstPerson)
            {
                desiredPos = targetPos + glm::vec3(0.0f, 1.4f, 0.0f);
                lookTarget = desiredPos + target->transform.GetForward() * 10.0f;
            }

            float t = (dt > 0.0f) ? std::clamp(smooth * dt, 0.0f, 1.0f) : 1.0f;
            ioCamera.position = glm::mix(ioCamera.position, desiredPos, t);
            glm::vec3 dir = lookTarget - ioCamera.position;
            if (glm::length(dir) > 0.001f)
                ioCamera.forward = glm::normalize(dir);
        }
    }

    static void DrawBar(
        ImDrawList* dl,
        const ImVec2& pos,
        const ImVec2& size,
        float fraction,
        ImU32 fillColor,
        const char* label)
    {
        fraction = std::clamp(fraction, 0.0f, 1.0f);
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(12, 18, 28, 210), 4.0f);
        if (fraction > 0.0f)
        {
            dl->AddRectFilled(
                ImVec2(pos.x + 2, pos.y + 2),
                ImVec2(pos.x + 2 + (size.x - 4) * fraction, pos.y + size.y - 2),
                fillColor,
                3.0f);
        }
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(70, 140, 210, 190), 4.0f, 0, 1.5f);
        dl->AddText(ImVec2(pos.x + 8, pos.y + 3), IM_COL32(240, 248, 255, 255), label);
    }

    void GameSystem::RenderHUD(
        Scene& scene,
        const ImVec2& vpMin,
        const ImVec2& vpSize,
        ImDrawList* dl)
    {
        if (!dl || vpSize.x < 100.0f || vpSize.y < 100.0f)
            return;

        const Entity* player = nullptr;
        {
            auto players = scene.FindEntitiesByTag("Player");
            if (!players.empty())
                player = scene.GetEntity(players[0]);
        }

        const Entity* core = nullptr;
        {
            auto cores = scene.FindEntitiesByTag("Core");
            if (!cores.empty())
                core = scene.GetEntity(cores[0]);
        }

        char buf[160];

        // Top-Left Status Panel (adapts cleanly whether Core exists or not!)
        if (player || core)
        {
            float panelH = (player && core) ? 98.0f : (player ? 70.0f : 42.0f);
            ImVec2 p0(vpMin.x + 16.0f, vpMin.y + 16.0f);
            dl->AddRectFilled(p0, ImVec2(p0.x + 265.0f, p0.y + panelH), IM_COL32(8, 14, 24, 185), 6.0f);
            dl->AddRect(p0, ImVec2(p0.x + 265.0f, p0.y + panelH), IM_COL32(55, 130, 200, 160), 6.0f);

            float y = p0.y + 10.0f;
            if (player)
            {
                float hp = player->gameplay.health;
                float maxHp = std::max(1.0f, player->gameplay.maxHealth);
                float sh = player->gameplay.shield;
                float maxSh = std::max(1.0f, player->gameplay.maxShield);

                std::snprintf(buf, sizeof(buf), "%s: %.0f / %.0f", scene.hudHealthLabel.c_str(), hp, maxHp);
                DrawBar(dl, ImVec2(p0.x + 10.0f, y), ImVec2(245.0f, 22.0f), hp / maxHp, IM_COL32(45, 210, 115, 235), buf);
                y += 28.0f;

                std::snprintf(buf, sizeof(buf), "%s: %.0f%%", scene.hudShieldLabel.c_str(), (sh / maxSh) * 100.0f);
                DrawBar(dl, ImVec2(p0.x + 10.0f, y), ImVec2(245.0f, 22.0f), sh / maxSh, IM_COL32(40, 165, 245, 235), buf);
                y += 28.0f;
            }
            if (core)
            {
                float coreHp = core->gameplay.health;
                float coreMaxHp = std::max(1.0f, core->gameplay.maxHealth);
                std::snprintf(buf, sizeof(buf), "%s: %.0f / %.0f", scene.hudObjectiveLabel.c_str(), coreHp, coreMaxHp);
                DrawBar(dl, ImVec2(p0.x + 10.0f, y), ImVec2(245.0f, 22.0f), coreHp / coreMaxHp, IM_COL32(235, 175, 40, 235), buf);
            }
        }

        // Top-Center Scene & Wave/Status Badge
        ImVec2 topCenter(vpMin.x + vpSize.x * 0.5f - 165.0f, vpMin.y + 16.0f);
        dl->AddRectFilled(topCenter, ImVec2(topCenter.x + 330.0f, topCenter.y + 52.0f), IM_COL32(10, 18, 32, 200), 6.0f);
        dl->AddRect(topCenter, ImVec2(topCenter.x + 330.0f, topCenter.y + 52.0f), IM_COL32(80, 185, 255, 180), 6.0f);

        std::snprintf(buf, sizeof(buf), "%s", scene.sceneName.c_str());
        dl->AddText(ImVec2(topCenter.x + 16.0f, topCenter.y + 8.0f), IM_COL32(110, 225, 255, 255), buf);

        int remainingPickups = static_cast<int>(scene.FindEntitiesByTag("Crystal").size());
        if (scene.maxWaves > 1)
            std::snprintf(buf, sizeof(buf), "WAVE %d / %d   |   HOSTILES: %d", scene.wave, scene.maxWaves, scene.enemiesRemainingInWave);
        else if (scene.enemiesRemainingInWave > 0)
            std::snprintf(buf, sizeof(buf), "HOSTILES REMAINING: %d", scene.enemiesRemainingInWave);
        else if (remainingPickups > 0)
            std::snprintf(buf, sizeof(buf), "%s REMAINING: %d", scene.hudCollectibleLabel.c_str(), remainingPickups);
        else
            std::snprintf(buf, sizeof(buf), "ENTITIES: %zu", scene.GetEntities().size());
        dl->AddText(ImVec2(topCenter.x + 16.0f, topCenter.y + 28.0f), IM_COL32(255, 175, 115, 255), buf);

        // Top-Right Score & Stats Panel
        ImVec2 topRight(vpMin.x + vpSize.x - 230.0f, vpMin.y + 16.0f);
        dl->AddRectFilled(topRight, ImVec2(topRight.x + 214.0f, topRight.y + 72.0f), IM_COL32(8, 14, 24, 185), 6.0f);
        dl->AddRect(topRight, ImVec2(topRight.x + 214.0f, topRight.y + 72.0f), IM_COL32(55, 130, 200, 160), 6.0f);

        std::snprintf(buf, sizeof(buf), "SCORE:     %06d", scene.score);
        dl->AddText(ImVec2(topRight.x + 14.0f, topRight.y + 10.0f), IM_COL32(255, 225, 90, 255), buf);
        std::snprintf(buf, sizeof(buf), "%s:  %d", scene.hudCollectibleLabel.c_str(), scene.crystalsCollected);
        dl->AddText(ImVec2(topRight.x + 14.0f, topRight.y + 30.0f), IM_COL32(100, 235, 255, 255), buf);
        std::snprintf(buf, sizeof(buf), "DEFEATED:  %d", scene.enemiesKilled);
        dl->AddText(ImVec2(topRight.x + 14.0f, topRight.y + 50.0f), IM_COL32(220, 230, 245, 255), buf);

        // Banner Alert
        if (scene.hudBannerTimer > 0.0f && !scene.hudBannerText.empty())
        {
            ImVec2 textSize = ImGui::CalcTextSize(scene.hudBannerText.c_str());
            ImVec2 bMin(vpMin.x + (vpSize.x - textSize.x) * 0.5f - 20.0f, vpMin.y + 82.0f);
            ImVec2 bMax(bMin.x + textSize.x + 40.0f, bMin.y + 34.0f);
            ImU32 borderCol = scene.gameWon ? IM_COL32(80, 255, 140, 230) : (scene.gameOver ? IM_COL32(255, 80, 80, 230) : IM_COL32(90, 210, 255, 220));
            dl->AddRectFilled(bMin, bMax, IM_COL32(12, 22, 38, 225), 6.0f);
            dl->AddRect(bMin, bMax, borderCol, 6.0f, 0, 2.0f);
            dl->AddText(ImVec2(bMin.x + 20.0f, bMin.y + 9.0f), borderCol, scene.hudBannerText.c_str());
        }

        // Bottom Controls Bar
        const char* controls = scene.hudControlsText.empty()
            ? "[W/S] Move   [A/D] Turn   [SPACE] Action   [SHIFT] Boost   [R] Restart"
            : scene.hudControlsText.c_str();
        ImVec2 ctrlSize = ImGui::CalcTextSize(controls);
        ImVec2 cMin(vpMin.x + (vpSize.x - ctrlSize.x) * 0.5f - 14.0f, vpMin.y + vpSize.y - 36.0f);
        ImVec2 cMax(cMin.x + ctrlSize.x + 28.0f, cMin.y + 26.0f);
        dl->AddRectFilled(cMin, cMax, IM_COL32(8, 12, 20, 185), 5.0f);
        dl->AddText(ImVec2(cMin.x + 14.0f, cMin.y + 5.0f), IM_COL32(190, 215, 240, 230), controls);
    }

    static void CopyDirectoryFilesIfExist(const std::filesystem::path& srcDir, const std::filesystem::path& dstDir)
    {
        if (!std::filesystem::exists(srcDir) || !std::filesystem::is_directory(srcDir))
            return;
        std::filesystem::create_directories(dstDir);
        for (const auto& entry : std::filesystem::directory_iterator(srcDir))
        {
            if (entry.is_regular_file())
            {
                std::filesystem::copy_file(
                    entry.path(),
                    dstDir / entry.path().filename(),
                    std::filesystem::copy_options::overwrite_existing);
            }
        }
    }

    bool GameSystem::ExportStandaloneGame(
        const Scene& currentScene,
        const GameProjectConfig& config,
        const std::string& exportDir,
        std::string& outStatusMessage)
    {
        try
        {
            std::filesystem::path outPath(exportDir);
            std::filesystem::create_directories(outPath);
            std::filesystem::create_directories(outPath / "assets" / "scenes");
            std::filesystem::create_directories(outPath / "assets" / "scripts");
            std::filesystem::create_directories(outPath / "assets" / "audio");
            std::filesystem::create_directories(outPath / "assets" / "shaders");
            std::filesystem::create_directories(outPath / "saves");

            // 1. Save current scene + any sibling project scenes
            std::string sceneFilename = config.projectName.empty() ? "startup.hscene" : (config.projectName + ".hscene");
            std::string relScenePath = "assets/scenes/" + sceneFilename;
            if (!config.projectRootDir.empty())
                CopyDirectoryFilesIfExist(std::filesystem::path(config.projectRootDir) / "scenes", outPath / "assets" / "scenes");
            SceneSerializer::SaveToFile(currentScene, (outPath / relScenePath).string());

            // 1b. Optional BadLevel assets (only when requested by project)
            if (config.enableBadLevel)
            {
                CopyDirectoryFilesIfExist("assets/levels", outPath / "assets" / "levels");
                if (!config.projectRootDir.empty())
                    CopyDirectoryFilesIfExist(std::filesystem::path(config.projectRootDir) / "levels", outPath / "assets" / "levels");
            }

            // 2. Write standalone project.hproject (and game.json for compatibility)
            GameProjectConfig exportedCfg = config;
            exportedCfg.projectRootDir = ".";
            exportedCfg.startupScene = relScenePath;
            SceneSerializer::SaveProjectConfig(exportedCfg, (outPath / "project.hproject").string());
            SceneSerializer::SaveProjectConfig(exportedCfg, (outPath / "game.json").string());

            // 3. Copy engine starter scripts + overlay project-specific scripts
            ScriptEngine::EnsureDefaultLuaScripts("assets/scripts");
            CopyDirectoryFilesIfExist("assets/scripts", outPath / "assets" / "scripts");
            if (!config.projectRootDir.empty())
                CopyDirectoryFilesIfExist(std::filesystem::path(config.projectRootDir) / "scripts", outPath / "assets" / "scripts");

            // 4. Generate/copy Audio WAV files
            AudioEngine::EnsureBuiltInSoundAssets((outPath / "assets" / "audio").string());
            CopyDirectoryFilesIfExist("assets/audio", outPath / "assets" / "audio");
            if (!config.projectRootDir.empty())
                CopyDirectoryFilesIfExist(std::filesystem::path(config.projectRootDir) / "audio", outPath / "assets" / "audio");

            // 5. Copy Shaders
            CopyDirectoryFilesIfExist("assets/shaders", outPath / "assets" / "shaders");

            // 6. Locate runtime binary: prefer matching project binary (e.g. ElderSlipgateFPS.exe) or fallback to HargonRuntime.exe
            std::string exeName = config.executableName.empty() ? (config.projectName + ".exe") : config.executableName;
            std::vector<std::filesystem::path> candidateBins = {
                std::filesystem::path("build/Release") / exeName,
                std::filesystem::path("build/Debug") / exeName,
                std::filesystem::path("build") / exeName,
                "build/Release/HargonRuntime.exe",
                "build/Debug/HargonRuntime.exe",
                "build/HargonRuntime.exe",
                "HargonRuntime.exe",
                "../Release/HargonRuntime.exe",
                "Release/HargonRuntime.exe"
            };

            bool copiedBinary = false;
            std::filesystem::path destExe = outPath / exeName;
            for (const auto& cand : candidateBins)
            {
                if (std::filesystem::exists(cand))
                {
                    std::filesystem::copy_file(cand, destExe, std::filesystem::copy_options::overwrite_existing);
                    copiedBinary = true;
                    break;
                }
            }

            // Write README_PLAY.txt
            {
                std::ofstream readme(outPath / "README_PLAY.txt", std::ios::out | std::ios::trunc);
                readme << "============================================================\n";
                readme << "  " << config.gameTitle << " (" << config.genre << ")\n";
                readme << "  Built & Exported with Hargon 3D Game Engine\n";
                readme << "  Project: " << config.projectName << " | Module: " << config.gameplayModule << "\n";
                readme << "============================================================\n\n";
                readme << "HOW TO PLAY:\n";
                readme << "  Run " << exeName << "\n";
            }

            if (copiedBinary)
                outStatusMessage = "Exported standalone project '" + config.gameTitle + "' to '" + std::filesystem::absolute(outPath).string() + "' (" + exeName + ")!";
            else
                outStatusMessage = "Exported project '" + config.gameTitle + "' assets & project.hproject to '" + std::filesystem::absolute(outPath).string() + "'.";

            Log::Editor(outStatusMessage);
            return true;
        }
        catch (const std::exception& ex)
        {
            outStatusMessage = std::string("Export failed: ") + ex.what();
            Log::Error(outStatusMessage);
            return false;
        }
    }
}
