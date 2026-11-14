#include "GameSystem.h"
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
            scene.BuildStarforgeOutpostScene();
            audio.PlaySound2D("wave_start", 0.8f, 1.0f);
            return;
        }

        if (scene.hudBannerTimer > 0.0f)
            scene.hudBannerTimer = std::max(0.0f, scene.hudBannerTimer - dt);

        if (scene.gameOver || scene.gameWon)
        {
            renderer.UpdateParticles(dt);
            return;
        }

        // 1. Update Lua Scripts
        scripts.UpdateScripts(scene, dt);

        // 2. Step Physics & Collisions
        physics.Step(scene, dt);

        // 3. Process Projectile Lifetimes & Player Shield Regen
        for (auto& e : scene.GetEntities())
        {
            if (!e.alive || !e.gameplay.enabled)
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
                audio.SetListenerTransform(e.transform.position, e.transform.GetForward());
            }
        }

        // 4. Process Collision Events
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

            // Player/Turret Projectile hits Enemy Drone
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

                    if ((scene.enemiesKilled % 2) == 0)
                    {
                        scene.SpawnEnergyCrystal(deathPos, (scene.enemiesKilled % 4) == 0);
                    }
                }
            }
            // Enemy Projectile hits Player Rover
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
            // Enemy Projectile hits Starforge Core
            else if (matchPair("EnemyProjectile", "Core", p1, p2))
            {
                p2->gameplay.health = std::max(0.0f, p2->gameplay.health - p1->gameplay.damage);
                p1->alive = false;
                renderer.SpawnParticleBurst(col.contactPoint, glm::vec3(1.0f, 0.35f, 0.15f), 12, 8.0f);
                audio.PlaySound3D("hit", col.contactPoint, 0.75f, 0.9f);
            }
            // Player Rover collects Energy Crystal
            else if (matchPair("Player", "Crystal", p1, p2))
            {
                p1->gameplay.health = std::min(p1->gameplay.maxHealth, p1->gameplay.health + p2->gameplay.health);
                p1->gameplay.shield = std::min(p1->gameplay.maxShield, p1->gameplay.shield + 25.0f);
                scene.score += p2->gameplay.scoreValue;
                ++scene.crystalsCollected;
                renderer.SpawnParticleBurst(p2->transform.position, glm::vec3(0.3f, 0.98f, 1.0f), 18, 9.0f);
                audio.PlaySound2D("pickup", 0.85f, 1.0f);
                p2->alive = false;
            }
            // Enemy Drone rams Player Rover or Core
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

        // 5. Check Player / Core Survival & Wave Progression
        Entity* player = scene.FindEntityByName("PlayerRover");
        Entity* core = scene.FindEntityByName("StarforgeCore");
        if ((player && player->gameplay.health <= 0.0f) || (core && core->gameplay.health <= 0.0f))
        {
            scene.gameOver = true;
            scene.hudBannerText = "OUTPOST OVERRUN! PRESS [R] TO RESTART MISSION";
            scene.hudBannerTimer = 999.0f;
            audio.PlaySound2D("explosion", 1.0f, 0.7f);
            return;
        }

        scene.enemiesRemainingInWave = static_cast<int>(scene.FindEntitiesByTag("Enemy").size());
        if (scene.enemiesRemainingInWave == 0)
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
                        scene.SpawnEnemyDrone(pos, scene.wave);
                    }
                    scene.SpawnEnergyCrystal(glm::vec3(-10.0f, 0.95f, 10.0f), true);
                    scene.SpawnEnergyCrystal(glm::vec3( 10.0f, 0.95f, 10.0f), false);

                    scene.enemiesRemainingInWave = spawnCount;
                    scene.hudBannerText = "WAVE " + std::to_string(scene.wave) + " INCOMING: " + std::to_string(spawnCount) + " SEEKER DRONES!";
                    scene.hudBannerTimer = 3.5f;
                    audio.PlaySound2D("wave_start", 0.9f, 1.0f);
                    Log::Gameplay("Started Wave " + std::to_string(scene.wave) + " with " + std::to_string(spawnCount) + " drones.");
                }
            }
            else
            {
                scene.gameWon = true;
                scene.score += 1000;
                scene.hudBannerText = "VICTORY! STARFORGE OUTPOST SECURED!";
                scene.hudBannerTimer = 999.0f;
                audio.PlaySound2D("victory", 0.95f, 1.0f);
                if (core)
                    renderer.SpawnParticleBurst(core->transform.position + glm::vec3(0, 2.5f, 0), glm::vec3(0.3f, 1.0f, 0.6f), 45, 14.0f);
                Log::Gameplay("Player won the game! Final Score: " + std::to_string(scene.score));
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
        std::string targetName = "PlayerRover";

        if (camEntity)
        {
            ioCamera.fovDegrees = camEntity->camera.fov;
            ioCamera.nearClip = camEntity->camera.nearClip;
            ioCamera.farClip = camEntity->camera.farClip;
            followOffset = camEntity->camera.followOffset;
            lookOffset = camEntity->camera.followLookOffset;
            smooth = camEntity->camera.smoothSpeed;
            if (!camEntity->camera.followTargetName.empty())
                targetName = camEntity->camera.followTargetName;
        }

        const Entity* target = const_cast<Scene&>(scene).FindEntityByName(targetName);
        if (target)
        {
            glm::vec3 desiredPos = target->transform.position + followOffset;
            glm::vec3 lookTarget = target->transform.position + lookOffset;
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

        const Entity* player = scene.FindEntityByName("PlayerRover");
        const Entity* core = scene.FindEntityByName("StarforgeCore");

        float playerHp = player ? player->gameplay.health : 0.0f;
        float playerMaxHp = player ? std::max(1.0f, player->gameplay.maxHealth) : 100.0f;
        float playerShield = player ? player->gameplay.shield : 0.0f;
        float playerMaxShield = player ? std::max(1.0f, player->gameplay.maxShield) : 100.0f;
        float coreHp = core ? core->gameplay.health : 0.0f;
        float coreMaxHp = core ? std::max(1.0f, core->gameplay.maxHealth) : 250.0f;

        // Top-Left Status Panel (Hull, Shield, Core)
        ImVec2 p0(vpMin.x + 16.0f, vpMin.y + 16.0f);
        dl->AddRectFilled(p0, ImVec2(p0.x + 255.0f, p0.y + 98.0f), IM_COL32(8, 14, 24, 185), 6.0f);
        dl->AddRect(p0, ImVec2(p0.x + 255.0f, p0.y + 98.0f), IM_COL32(55, 130, 200, 160), 6.0f);

        char buf[128];
        std::snprintf(buf, sizeof(buf), "ROVER HULL: %.0f / %.0f", playerHp, playerMaxHp);
        DrawBar(dl, ImVec2(p0.x + 10.0f, p0.y + 10.0f), ImVec2(235.0f, 22.0f), playerHp / playerMaxHp, IM_COL32(45, 210, 115, 235), buf);

        std::snprintf(buf, sizeof(buf), "ENERGY SHIELD: %.0f%%", (playerShield / playerMaxShield) * 100.0f);
        DrawBar(dl, ImVec2(p0.x + 10.0f, p0.y + 38.0f), ImVec2(235.0f, 22.0f), playerShield / playerMaxShield, IM_COL32(40, 165, 245, 235), buf);

        std::snprintf(buf, sizeof(buf), "STARFORGE CORE: %.0f / %.0f", coreHp, coreMaxHp);
        DrawBar(dl, ImVec2(p0.x + 10.0f, p0.y + 66.0f), ImVec2(235.0f, 22.0f), coreHp / coreMaxHp, IM_COL32(235, 175, 40, 235), buf);

        // Top-Center Wave & Hostiles Badge
        ImVec2 topCenter(vpMin.x + vpSize.x * 0.5f - 135.0f, vpMin.y + 16.0f);
        dl->AddRectFilled(topCenter, ImVec2(topCenter.x + 270.0f, topCenter.y + 52.0f), IM_COL32(10, 18, 32, 200), 6.0f);
        dl->AddRect(topCenter, ImVec2(topCenter.x + 270.0f, topCenter.y + 52.0f), IM_COL32(80, 185, 255, 180), 6.0f);

        std::snprintf(buf, sizeof(buf), "WAVE %d / %d", scene.wave, scene.maxWaves);
        dl->AddText(ImVec2(topCenter.x + 92.0f, topCenter.y + 8.0f), IM_COL32(110, 225, 255, 255), buf);

        std::snprintf(buf, sizeof(buf), "SEEKER DRONES ACTIVE: %d", scene.enemiesRemainingInWave);
        dl->AddText(ImVec2(topCenter.x + 42.0f, topCenter.y + 28.0f), IM_COL32(255, 130, 120, 255), buf);

        // Top-Right Score & Stats Panel
        ImVec2 topRight(vpMin.x + vpSize.x - 230.0f, vpMin.y + 16.0f);
        dl->AddRectFilled(topRight, ImVec2(topRight.x + 214.0f, topRight.y + 72.0f), IM_COL32(8, 14, 24, 185), 6.0f);
        dl->AddRect(topRight, ImVec2(topRight.x + 214.0f, topRight.y + 72.0f), IM_COL32(55, 130, 200, 160), 6.0f);

        std::snprintf(buf, sizeof(buf), "SCORE:     %06d", scene.score);
        dl->AddText(ImVec2(topRight.x + 14.0f, topRight.y + 10.0f), IM_COL32(255, 225, 90, 255), buf);
        std::snprintf(buf, sizeof(buf), "CRYSTALS:  %d", scene.crystalsCollected);
        dl->AddText(ImVec2(topRight.x + 14.0f, topRight.y + 30.0f), IM_COL32(100, 235, 255, 255), buf);
        std::snprintf(buf, sizeof(buf), "DESTROYED: %d", scene.enemiesKilled);
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
        const char* controls = "[W/S] Thrust   [A/D] Steer   [SPACE] Fire Plasma Cannons   [SHIFT] Boost   [R] Restart";
        ImVec2 ctrlSize = ImGui::CalcTextSize(controls);
        ImVec2 cMin(vpMin.x + (vpSize.x - ctrlSize.x) * 0.5f - 14.0f, vpMin.y + vpSize.y - 36.0f);
        ImVec2 cMax(cMin.x + ctrlSize.x + 28.0f, cMin.y + 26.0f);
        dl->AddRectFilled(cMin, cMax, IM_COL32(8, 12, 20, 185), 5.0f);
        dl->AddText(ImVec2(cMin.x + 14.0f, cMin.y + 5.0f), IM_COL32(190, 215, 240, 230), controls);
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
            std::filesystem::create_directories(outPath / "assets" / "levels");
            std::filesystem::create_directories(outPath / "assets" / "scripts");
            std::filesystem::create_directories(outPath / "assets" / "audio");
            std::filesystem::create_directories(outPath / "assets" / "shaders");
            std::filesystem::create_directories(outPath / "saves");

            // 1. Save current scene into exported assets/scenes/starforge_outpost.hscene
            std::string relScenePath = "assets/scenes/starforge_outpost.hscene";
            SceneSerializer::SaveToFile(currentScene, (outPath / relScenePath).string());

            // 1b. Copy all BadLevel generated campaign levels (c1m1..c1m5, c2m1..c2m5, campaigns.json)
            if (std::filesystem::exists("assets/levels"))
            {
                for (const auto& entry : std::filesystem::directory_iterator("assets/levels"))
                {
                    if (entry.is_regular_file())
                    {
                        std::filesystem::copy_file(
                            entry.path(),
                            outPath / "assets" / "levels" / entry.path().filename(),
                            std::filesystem::copy_options::overwrite_existing);
                    }
                }
            }

            // 2. Write game.json
            GameProjectConfig exportedCfg = config;
            exportedCfg.startupScene = relScenePath;
            SceneSerializer::SaveProjectConfig(exportedCfg, (outPath / "game.json").string());

            // 3. Copy Lua scripts
            ScriptEngine::EnsureDefaultLuaScripts("assets/scripts");
            for (const auto& entry : std::filesystem::directory_iterator("assets/scripts"))
            {
                if (entry.is_regular_file())
                {
                    std::filesystem::copy_file(
                        entry.path(),
                        outPath / "assets" / "scripts" / entry.path().filename(),
                        std::filesystem::copy_options::overwrite_existing);
                }
            }

            // 4. Generate/copy Audio WAV files
            AudioEngine::EnsureBuiltInSoundAssets((outPath / "assets" / "audio").string());
            if (std::filesystem::exists("assets/audio"))
            {
                for (const auto& entry : std::filesystem::directory_iterator("assets/audio"))
                {
                    if (entry.is_regular_file())
                    {
                        std::filesystem::copy_file(
                            entry.path(),
                            outPath / "assets" / "audio" / entry.path().filename(),
                            std::filesystem::copy_options::overwrite_existing);
                    }
                }
            }

            // 5. Copy Shaders
            if (std::filesystem::exists("assets/shaders"))
            {
                for (const auto& entry : std::filesystem::directory_iterator("assets/shaders"))
                {
                    if (entry.is_regular_file())
                    {
                        std::filesystem::copy_file(
                            entry.path(),
                            outPath / "assets" / "shaders" / entry.path().filename(),
                            std::filesystem::copy_options::overwrite_existing);
                    }
                }
            }

            // 6. Locate and copy HargonRuntime executable as StarforgeDefender.exe
            std::vector<std::filesystem::path> candidateBins = {
                "build/Release/HargonRuntime.exe",
                "build/Debug/HargonRuntime.exe",
                "build/HargonRuntime.exe",
                "HargonRuntime.exe",
                "../Release/HargonRuntime.exe",
                "Release/HargonRuntime.exe"
            };

            bool copiedBinary = false;
            std::filesystem::path destExe = outPath / "ElderSlipgateFPS.exe";
            std::filesystem::path destExeCompat = outPath / "StarforgeDefender.exe";
            for (const auto& cand : candidateBins)
            {
                if (std::filesystem::exists(cand))
                {
                    std::filesystem::copy_file(cand, destExe, std::filesystem::copy_options::overwrite_existing);
                    std::filesystem::copy_file(cand, destExeCompat, std::filesystem::copy_options::overwrite_existing);
                    copiedBinary = true;
                    break;
                }
            }

            // Write README_PLAY.txt
            {
                std::ofstream readme(outPath / "README_PLAY.txt", std::ios::out | std::ios::trunc);
                readme << "============================================================\n";
                readme << "  ELDER SLIPGATE: QUAKE-INSPIRED SINGLE-PLAYER FPS\n";
                readme << "  Built & Exported with Hargon 3D Game Engine + BadLevel Generator\n";
                readme << "  RHI: NVIDIA NVRHI (Vulkan / D3D11) | Audio: miniaudio | Scripting: Lua 5.4 | Window: GLFW\n";
                readme << "============================================================\n\n";
                readme << "HOW TO PLAY:\n";
                readme << "  Run ElderSlipgateFPS.exe\n\n";
                readme << "CAMPAIGNS & LEVELS (Generated via BadLevel):\n";
                readme << "  Campaign 1: The Elder Ward (c1m1 - c1m5: Gothic, Castle, Egyptian Vaults)\n";
                readme << "  Campaign 2: The Iron Citadel (c2m1 - c2m5: Bunker, Industrial, Compound, Sci-Fi)\n\n";
                readme << "WEAPONS (Keys 1-5):\n";
                readme << "  [1] Runic Battle-Axe\n";
                readme << "  [2] Double-Barrel Buckshot Shotgun\n";
                readme << "  [3] Super Perforator Nailgun\n";
                readme << "  [4] Ogre Rocket Launcher\n";
                readme << "  [5] Thunderbolt Lightning Shaft\n\n";
                readme << "CONTROLS:\n";
                readme << "  [W/A/S/D]           : Move & Strafe\n";
                readme << "  [Mouse / Arrows]    : Look / Turn\n";
                readme << "  [Left Mouse / F]    : Fire Active Weapon\n";
                readme << "  [SPACE]             : Jump\n";
                readme << "  [1 - 5 / Scroll]    : Select Weapon\n";
                readme << "  [F5 / F9]           : QuickSave / QuickLoad\n";
                readme << "  [ESC]               : Game Menu, Campaign Select, Save/Load, Graphics & Sound Settings\n";
            }

            if (copiedBinary)
            {
                outStatusMessage = "Exported standalone FPS game to '" + std::filesystem::absolute(outPath).string() + "' (ElderSlipgateFPS.exe + 10 BadLevel Maps + assets)!";
                Log::Editor(outStatusMessage);
                return true;
            }
            else
            {
                outStatusMessage = "Exported game assets & game.json to '" + std::filesystem::absolute(outPath).string() + "' (HargonRuntime.exe will be placed once built).";
                Log::Editor(outStatusMessage);
                return true;
            }
        }
        catch (const std::exception& ex)
        {
            outStatusMessage = std::string("Export failed: ") + ex.what();
            Log::Error(outStatusMessage);
            return false;
        }
    }
}
