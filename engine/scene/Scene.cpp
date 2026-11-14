#include "Scene.h"
#include "../core/Log.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Hargon
{
    void Scene::Clear()
    {
        m_Entities.clear();
        m_Entities.reserve(512);
        m_NextEntityID = 1;
        score = 0;
        wave = 1;
        enemiesRemainingInWave = 0;
        enemiesKilled = 0;
        crystalsCollected = 0;
        waveCooldownTimer = 0.0f;
        gameOver = false;
        gameWon = false;
        hudBannerText = "WAVE 1: DEFEND THE STARFORGE CORE!";
        hudBannerTimer = 3.5f;
    }

    Entity& Scene::CreateEntity(const std::string& name, const std::string& tag)
    {
        if (m_Entities.capacity() < 512)
            m_Entities.reserve(512);

        Entity e;
        e.id = m_NextEntityID++;
        e.alive = true;
        e.tag.name = name;
        e.tag.tag = tag;
        if (!tag.empty() && tag != "Default")
            e.tag.AddToGroup(tag);
        m_Entities.push_back(e);
        return m_Entities.back();
    }

    EntityID Scene::DuplicateEntity(EntityID sourceId)
    {
        const Entity* src = GetEntity(sourceId);
        if (!src) return kInvalidEntity;

        Entity copy = *src;
        copy.id = m_NextEntityID++;
        copy.tag.name = src->tag.name + "_Copy";
        copy.transform.position += glm::vec3(1.5f, 0.0f, 1.5f);
        copy.luaScript.initialized = false;
        copy.childrenIds.clear();
        m_Entities.push_back(copy);
        if (copy.parentId != kInvalidEntity)
        {
            if (Entity* p = GetEntity(copy.parentId))
                p->childrenIds.push_back(copy.id);
        }
        return copy.id;
    }

    void Scene::DestroyEntity(EntityID id)
    {
        for (auto& e : m_Entities)
        {
            if (e.id == id)
            {
                if (e.parentId != kInvalidEntity)
                {
                    if (Entity* p = GetEntity(e.parentId))
                    {
                        auto& c = p->childrenIds;
                        c.erase(std::remove(c.begin(), c.end(), id), c.end());
                    }
                }
                for (EntityID childId : e.childrenIds)
                {
                    if (Entity* ch = GetEntity(childId))
                        ch->parentId = kInvalidEntity;
                }
                e.childrenIds.clear();
                e.alive = false;
                break;
            }
        }
    }

    void Scene::FlushDestroyedEntities()
    {
        m_Entities.erase(
            std::remove_if(m_Entities.begin(), m_Entities.end(), [](const Entity& e) { return !e.alive; }),
            m_Entities.end());
    }

    Entity* Scene::GetEntity(EntityID id)
    {
        if (id == kInvalidEntity) return nullptr;
        for (auto& e : m_Entities)
        {
            if (e.alive && e.id == id)
                return &e;
        }
        return nullptr;
    }

    const Entity* Scene::GetEntity(EntityID id) const
    {
        if (id == kInvalidEntity) return nullptr;
        for (const auto& e : m_Entities)
        {
            if (e.alive && e.id == id)
                return &e;
        }
        return nullptr;
    }

    Entity* Scene::FindEntityByName(const std::string& name)
    {
        for (auto& e : m_Entities)
        {
            if (e.alive && e.tag.name == name)
                return &e;
        }
        return nullptr;
    }

    std::vector<EntityID> Scene::FindEntitiesByTag(const std::string& tag) const
    {
        std::vector<EntityID> result;
        for (const auto& e : m_Entities)
        {
            if (e.alive && e.tag.tag == tag)
                result.push_back(e.id);
        }
        return result;
    }

    bool Scene::SetParent(EntityID childId, EntityID newParentId)
    {
        if (childId == kInvalidEntity || childId == newParentId)
            return false;

        Entity* child = GetEntity(childId);
        if (!child)
            return false;

        // Check for cycle in parent chain
        EntityID cursor = newParentId;
        int guard = 0;
        while (cursor != kInvalidEntity && guard < 64)
        {
            if (cursor == childId)
                return false;
            const Entity* p = GetEntity(cursor);
            cursor = p ? p->parentId : kInvalidEntity;
            ++guard;
        }

        // Detach from old parent
        if (child->parentId != kInvalidEntity)
        {
            if (Entity* oldP = GetEntity(child->parentId))
            {
                auto& ch = oldP->childrenIds;
                ch.erase(std::remove(ch.begin(), ch.end(), childId), ch.end());
            }
        }

        child->parentId = newParentId;
        if (newParentId != kInvalidEntity)
        {
            if (Entity* newP = GetEntity(newParentId))
            {
                if (std::find(newP->childrenIds.begin(), newP->childrenIds.end(), childId) == newP->childrenIds.end())
                    newP->childrenIds.push_back(childId);
            }
        }
        return true;
    }

    glm::mat4 Scene::GetWorldMatrix(const Entity& e) const
    {
        glm::mat4 mat = e.transform.GetTransformMatrix();
        EntityID pId = e.parentId;
        int depth = 0;
        while (pId != kInvalidEntity && depth < 16)
        {
            const Entity* parent = GetEntity(pId);
            if (!parent)
                break;
            mat = parent->transform.GetTransformMatrix() * mat;
            pId = parent->parentId;
            ++depth;
        }
        return mat;
    }

    glm::vec3 Scene::GetWorldPosition(const Entity& e) const
    {
        if (e.parentId == kInvalidEntity)
            return e.transform.position;
        glm::mat4 wm = GetWorldMatrix(e);
        return glm::vec3(wm[3]);
    }

    std::vector<EntityID> Scene::GetEntitiesInGroup(const std::string& group) const
    {
        std::vector<EntityID> result;
        for (const auto& e : m_Entities)
        {
            if (e.alive && (e.tag.IsInGroup(group) || e.tag.tag == group))
                result.push_back(e.id);
        }
        return result;
    }

    void Scene::ConnectSignal(const std::string& signalName, SignalCallback cb)
    {
        if (!signalName.empty() && cb)
            m_SignalListeners[signalName].push_back(std::move(cb));
    }

    void Scene::EmitSignal(const std::string& signalName, EntityID emitterId, const std::string& payload)
    {
        std::string entry = "[Signal] " + signalName + " (from Entity " + std::to_string(emitterId) + ")" +
                            (payload.empty() ? "" : (" : " + payload));
        m_RecentSignals.push_back(entry);
        if (m_RecentSignals.size() > 32)
            m_RecentSignals.erase(m_RecentSignals.begin());

        auto it = m_SignalListeners.find(signalName);
        if (it != m_SignalListeners.end())
        {
            auto listenersCopy = it->second;
            for (auto& cb : listenersCopy)
            {
                if (cb) cb(emitterId, payload);
            }
        }
    }

    static bool RayIntersectAABB(const glm::vec3& orig, const glm::vec3& dir, const glm::vec3& bmin, const glm::vec3& bmax, float& tHit)
    {
        float tmin = -std::numeric_limits<float>::infinity();
        float tmax =  std::numeric_limits<float>::infinity();

        for (int a = 0; a < 3; ++a)
        {
            if (std::abs(dir[a]) < 1e-6f)
            {
                if (orig[a] < bmin[a] || orig[a] > bmax[a])
                    return false;
            }
            else
            {
                float invD = 1.0f / dir[a];
                float t0 = (bmin[a] - orig[a]) * invD;
                float t1 = (bmax[a] - orig[a]) * invD;
                if (t0 > t1) std::swap(t0, t1);
                tmin = std::max(tmin, t0);
                tmax = std::min(tmax, t1);
                if (tmax < tmin) return false;
            }
        }

        if (tmax < 0.0f) return false;
        tHit = (tmin >= 0.0f) ? tmin : tmax;
        return true;
    }

    EntityID Scene::Raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float* outDist) const
    {
        float bestDist = std::numeric_limits<float>::max();
        EntityID bestEntity = kInvalidEntity;

        for (const auto& e : m_Entities)
        {
            if (!e.alive || !e.mesh.enabled)
                continue;

            // Skip huge ground plane unless nothing else is hit
            bool isGround = (e.tag.name == "ArenaGround");

            glm::vec3 halfExt = e.transform.scale * 0.55f;
            halfExt = glm::max(halfExt, glm::vec3(0.25f));
            glm::vec3 bmin = e.transform.position - halfExt;
            glm::vec3 bmax = e.transform.position + halfExt;

            float t = 0.0f;
            if (RayIntersectAABB(rayOrigin, rayDir, bmin, bmax, t))
            {
                if (isGround)
                    t += 500.0f; // deprioritize floor so objects on it are easy to pick

                if (t < bestDist)
                {
                    bestDist = t;
                    bestEntity = e.id;
                }
            }
        }

        if (outDist && bestEntity != kInvalidEntity)
            *outDist = bestDist;
        return bestEntity;
    }

    Entity& Scene::SpawnEnemyDrone(const glm::vec3& pos, int waveLevel)
    {
        int idx = static_cast<int>(FindEntitiesByTag("Enemy").size()) + 1;
        Entity& drone = CreateEntity("SeekerDrone_" + std::to_string(m_NextEntityID), "Enemy");
        drone.transform.position = pos;
        drone.transform.scale = glm::vec3(1.25f);

        drone.mesh.enabled = true;
        drone.mesh.primitive = MeshPrimitive::Drone;

        drone.material.albedoColor = glm::vec4(0.92f, 0.20f, 0.24f, 1.0f);
        drone.material.metallic = 0.65f;
        drone.material.roughness = 0.25f;
        drone.material.emissiveColor = glm::vec3(1.0f, 0.18f, 0.12f);
        drone.material.emissiveStrength = 1.6f;
        drone.material.patternType = 3;

        drone.light.enabled = true;
        drone.light.isDirectional = false;
        drone.light.color = glm::vec3(1.0f, 0.22f, 0.15f);
        drone.light.intensity = 2.0f;
        drone.light.range = 8.5f;

        drone.rigidBody.enabled = true;
        drone.rigidBody.mass = 1.2f;
        drone.rigidBody.drag = 3.0f;
        drone.rigidBody.freezeY = true;

        drone.collider.enabled = true;
        drone.collider.isSphere = true;
        drone.collider.sphereRadius = 0.95f;

        drone.luaScript.enabled = true;
        drone.luaScript.scriptPath = "assets/scripts/enemy_drone.lua";

        drone.gameplay.enabled = true;
        drone.gameplay.maxHealth = 45.0f + (waveLevel - 1) * 20.0f;
        drone.gameplay.health = drone.gameplay.maxHealth;
        drone.gameplay.moveSpeed = 6.8f + (waveLevel - 1) * 1.1f;
        drone.gameplay.damage = 15.0f;
        drone.gameplay.fireRate = std::max(1.1f, 2.2f - waveLevel * 0.25f);
        drone.gameplay.fireCooldown = 0.8f + (idx % 3) * 0.4f;
        drone.gameplay.scoreValue = 150 * waveLevel;

        return drone;
    }

    Entity& Scene::SpawnEnergyCrystal(const glm::vec3& pos, bool golden)
    {
        Entity& crystal = CreateEntity(golden ? "SolarCrystal_" + std::to_string(m_NextEntityID) : "EnergyCrystal_" + std::to_string(m_NextEntityID), "Crystal");
        crystal.transform.position = pos;
        crystal.transform.scale = glm::vec3(golden ? 1.35f : 1.1f);

        crystal.mesh.enabled = true;
        crystal.mesh.primitive = MeshPrimitive::Crystal;

        if (golden)
        {
            crystal.material.albedoColor = glm::vec4(1.0f, 0.84f, 0.22f, 1.0f);
            crystal.material.emissiveColor = glm::vec3(1.0f, 0.72f, 0.15f);
            crystal.material.emissiveStrength = 2.4f;
        }
        else
        {
            crystal.material.albedoColor = glm::vec4(0.20f, 0.92f, 1.0f, 1.0f);
            crystal.material.emissiveColor = glm::vec3(0.15f, 0.85f, 1.0f);
            crystal.material.emissiveStrength = 2.0f;
        }
        crystal.material.metallic = 0.2f;
        crystal.material.roughness = 0.12f;
        crystal.material.patternType = 4;

        crystal.light.enabled = true;
        crystal.light.isDirectional = false;
        crystal.light.color = golden ? glm::vec3(1.0f, 0.8f, 0.2f) : glm::vec3(0.2f, 0.9f, 1.0f);
        crystal.light.intensity = 2.2f;
        crystal.light.range = 7.5f;

        crystal.collider.enabled = true;
        crystal.collider.isSphere = true;
        crystal.collider.sphereRadius = 1.05f;
        crystal.collider.isTrigger = true;

        crystal.luaScript.enabled = true;
        crystal.luaScript.scriptPath = "assets/scripts/crystal_pickup.lua";

        crystal.gameplay.enabled = true;
        crystal.gameplay.scoreValue = golden ? 250 : 100;
        crystal.gameplay.health = golden ? 35.0f : 20.0f; // heal amount

        return crystal;
    }

    Entity& Scene::SpawnProjectile(const glm::vec3& pos, const glm::vec3& dir, EntityID ownerId, bool isEnemyProjectile, float speed, float damage)
    {
        Entity& proj = CreateEntity(isEnemyProjectile ? "EnemyPlasmaBolt" : "PlayerPlasmaBolt", isEnemyProjectile ? "EnemyProjectile" : "Projectile");
        proj.transform.position = pos;
        proj.transform.scale = isEnemyProjectile ? glm::vec3(0.32f, 0.32f, 0.55f) : glm::vec3(0.28f, 0.28f, 0.75f);

        glm::vec3 normDir = glm::length(dir) > 0.001f ? glm::normalize(dir) : glm::vec3(0.0f, 0.0f, 1.0f);
        float yaw = std::atan2(normDir.x, normDir.z) * (180.0f / 3.14159265f);
        proj.transform.rotation = glm::vec3(0.0f, yaw, 0.0f);

        proj.mesh.enabled = true;
        proj.mesh.primitive = MeshPrimitive::Sphere;

        if (isEnemyProjectile)
        {
            proj.material.albedoColor = glm::vec4(1.0f, 0.35f, 0.15f, 1.0f);
            proj.material.emissiveColor = glm::vec3(1.0f, 0.25f, 0.08f);
            proj.material.emissiveStrength = 4.2f;
        }
        else
        {
            proj.material.albedoColor = glm::vec4(0.25f, 0.98f, 1.0f, 1.0f);
            proj.material.emissiveColor = glm::vec3(0.20f, 0.92f, 1.0f);
            proj.material.emissiveStrength = 4.5f;
        }
        proj.material.metallic = 0.0f;
        proj.material.roughness = 0.1f;

        proj.rigidBody.enabled = true;
        proj.rigidBody.drag = 0.0f;
        proj.rigidBody.freezeY = true;
        proj.rigidBody.velocity = normDir * speed;

        proj.collider.enabled = true;
        proj.collider.isSphere = true;
        proj.collider.sphereRadius = 0.42f;
        proj.collider.isTrigger = true;

        proj.gameplay.enabled = true;
        proj.gameplay.damage = damage;
        proj.gameplay.ownerId = ownerId;
        proj.gameplay.maxLifetime = 2.2f;
        proj.gameplay.lifetime = 0.0f;

        return proj;
    }

    void Scene::BuildStarforgeOutpostScene()
    {
        Clear();
        sceneName = "Starforge Outpost: Crystal Defense";

        // 1. Directional Sun Light
        {
            Entity& sun = CreateEntity("SolarDirectionalLight", "Light");
            sun.transform.position = glm::vec3(20.0f, 35.0f, -15.0f);
            sun.transform.rotation = glm::vec3(48.0f, -35.0f, 0.0f);
            sun.mesh.enabled = false;
            sun.light.enabled = true;
            sun.light.isDirectional = true;
            sun.light.color = glm::vec3(1.0f, 0.95f, 0.86f);
            sun.light.intensity = 2.6f;
        }

        // 2. Game Follow Camera
        {
            Entity& cam = CreateEntity("MainGameCamera", "Camera");
            cam.transform.position = glm::vec3(0.0f, 15.0f, -21.0f);
            cam.transform.rotation = glm::vec3(36.0f, 0.0f, 0.0f);
            cam.mesh.enabled = false;
            cam.camera.enabled = true;
            cam.camera.isPrimary = true;
            cam.camera.fov = 55.0f;
            cam.camera.followTargetName = "PlayerRover";
            cam.camera.followOffset = glm::vec3(0.0f, 13.5f, -14.5f);
            cam.camera.followLookOffset = glm::vec3(0.0f, 0.5f, 3.8f);
            cam.camera.smoothSpeed = 8.0f;
        }

        // 3. Sci-Fi Arena Ground
        {
            Entity& ground = CreateEntity("ArenaGround", "Environment");
            ground.transform.position = glm::vec3(0.0f, -0.5f, 0.0f);
            ground.transform.scale = glm::vec3(76.0f, 1.0f, 76.0f);
            ground.mesh.enabled = true;
            ground.mesh.primitive = MeshPrimitive::Cube;
            ground.material.albedoColor = glm::vec4(0.11f, 0.14f, 0.20f, 1.0f);
            ground.material.metallic = 0.35f;
            ground.material.roughness = 0.55f;
            ground.material.patternType = 1; // World-space Sci-Fi Grid
            ground.material.uvScale = 1.0f;
        }

        // 4. Perimeter Arena Walls
        struct WallDef { std::string name; glm::vec3 pos; glm::vec3 scale; };
        WallDef walls[4] = {
            { "NorthWall", {  0.0f, 1.2f,  37.0f }, { 76.0f, 2.8f, 1.5f } },
            { "SouthWall", {  0.0f, 1.2f, -37.0f }, { 76.0f, 2.8f, 1.5f } },
            { "EastWall",  {  37.0f, 1.2f,  0.0f }, { 1.5f,  2.8f, 76.0f } },
            { "WestWall",  { -37.0f, 1.2f,  0.0f }, { 1.5f,  2.8f, 76.0f } }
        };
        for (const auto& w : walls)
        {
            Entity& wall = CreateEntity(w.name, "Environment");
            wall.transform.position = w.pos;
            wall.transform.scale = w.scale;
            wall.mesh.enabled = true;
            wall.mesh.primitive = MeshPrimitive::Cube;
            wall.material.albedoColor = glm::vec4(0.18f, 0.24f, 0.34f, 1.0f);
            wall.material.metallic = 0.65f;
            wall.material.roughness = 0.30f;
            wall.material.emissiveColor = glm::vec3(0.08f, 0.45f, 0.75f);
            wall.material.emissiveStrength = 0.45f;
            wall.material.patternType = 3;
        }

        // 5. Central Starforge Reactor Core (Objective to defend!)
        {
            Entity& core = CreateEntity("StarforgeCore", "Core");
            core.transform.position = glm::vec3(0.0f, 1.4f, 0.0f);
            core.transform.scale = glm::vec3(2.4f, 2.6f, 2.4f);
            core.mesh.enabled = true;
            core.mesh.primitive = MeshPrimitive::Core;
            core.material.albedoColor = glm::vec4(0.30f, 0.85f, 1.0f, 1.0f);
            core.material.metallic = 0.60f;
            core.material.roughness = 0.18f;
            core.material.emissiveColor = glm::vec3(0.15f, 0.78f, 1.0f);
            core.material.emissiveStrength = 2.2f;

            core.light.enabled = true;
            core.light.isDirectional = false;
            core.light.color = glm::vec3(0.20f, 0.85f, 1.0f);
            core.light.intensity = 4.5f;
            core.light.range = 20.0f;

            core.collider.enabled = true;
            core.collider.isSphere = true;
            core.collider.sphereRadius = 2.2f;
            core.collider.isTrigger = false;

            core.rigidBody.enabled = true;
            core.rigidBody.isKinematic = true;

            core.luaScript.enabled = true;
            core.luaScript.scriptPath = "assets/scripts/core_reactor.lua";

            core.gameplay.enabled = true;
            core.gameplay.maxHealth = 250.0f;
            core.gameplay.health = 250.0f;
        }

        // 6. Player Hover-Rover
        {
            Entity& player = CreateEntity("PlayerRover", "Player");
            player.transform.position = glm::vec3(0.0f, 0.65f, -9.0f);
            player.transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
            player.transform.scale = glm::vec3(1.35f, 1.35f, 1.35f);

            player.mesh.enabled = true;
            player.mesh.primitive = MeshPrimitive::Rover;

            player.material.albedoColor = glm::vec4(0.22f, 0.68f, 0.98f, 1.0f);
            player.material.metallic = 0.70f;
            player.material.roughness = 0.22f;
            player.material.emissiveColor = glm::vec3(0.12f, 0.85f, 1.0f);
            player.material.emissiveStrength = 1.1f;
            player.material.patternType = 3;

            player.light.enabled = true;
            player.light.isDirectional = false;
            player.light.color = glm::vec3(0.25f, 0.88f, 1.0f);
            player.light.intensity = 2.8f;
            player.light.range = 12.0f;

            player.rigidBody.enabled = true;
            player.rigidBody.mass = 1.5f;
            player.rigidBody.drag = 6.5f;
            player.rigidBody.freezeY = true;

            player.collider.enabled = true;
            player.collider.isSphere = true;
            player.collider.sphereRadius = 1.15f;

            player.audioSource.enabled = true;
            player.audioSource.soundName = "laser";
            player.audioSource.volume = 0.75f;

            player.luaScript.enabled = true;
            player.luaScript.scriptPath = "assets/scripts/player_rover.lua";

            player.gameplay.enabled = true;
            player.gameplay.maxHealth = 100.0f;
            player.gameplay.health = 100.0f;
            player.gameplay.maxShield = 100.0f;
            player.gameplay.shield = 100.0f;
            player.gameplay.moveSpeed = 16.0f;
            player.gameplay.fireRate = 0.16f;
            player.gameplay.damage = 25.0f;
        }

        // 7. Two Automated Defense Turrets (Lua-scripted allies!)
        glm::vec3 turretPositions[2] = {
            { -6.5f, 0.85f,  4.0f },
            {  6.5f, 0.85f,  4.0f }
        };
        for (int i = 0; i < 2; ++i)
        {
            Entity& turret = CreateEntity("DefenseTurret_" + std::to_string(i + 1), "Turret");
            turret.transform.position = turretPositions[i];
            turret.transform.scale = glm::vec3(1.25f);
            turret.mesh.enabled = true;
            turret.mesh.primitive = MeshPrimitive::Turret;
            turret.material.albedoColor = glm::vec4(0.30f, 0.90f, 0.65f, 1.0f);
            turret.material.metallic = 0.65f;
            turret.material.roughness = 0.25f;
            turret.material.emissiveColor = glm::vec3(0.15f, 0.95f, 0.55f);
            turret.material.emissiveStrength = 1.0f;

            turret.collider.enabled = true;
            turret.collider.isSphere = true;
            turret.collider.sphereRadius = 1.0f;
            turret.rigidBody.enabled = true;
            turret.rigidBody.isKinematic = true;

            turret.luaScript.enabled = true;
            turret.luaScript.scriptPath = "assets/scripts/defense_turret.lua";

            turret.gameplay.enabled = true;
            turret.gameplay.fireRate = 0.95f;
            turret.gameplay.fireCooldown = 0.4f * (i + 1);
            turret.gameplay.damage = 15.0f;
        }

        // 8. Tactical Arena Pillars / Obstacles
        glm::vec3 pillarPositions[4] = {
            { -14.0f, 1.8f, -12.0f },
            {  14.0f, 1.8f, -12.0f },
            { -14.0f, 1.8f,  14.0f },
            {  14.0f, 1.8f,  14.0f }
        };
        for (int i = 0; i < 4; ++i)
        {
            Entity& pillar = CreateEntity("EnergyPylon_" + std::to_string(i + 1), "Obstacle");
            pillar.transform.position = pillarPositions[i];
            pillar.transform.scale = glm::vec3(2.2f, 3.8f, 2.2f);
            pillar.mesh.enabled = true;
            pillar.mesh.primitive = MeshPrimitive::Cylinder;
            pillar.material.albedoColor = glm::vec4(0.32f, 0.38f, 0.50f, 1.0f);
            pillar.material.metallic = 0.75f;
            pillar.material.roughness = 0.28f;
            pillar.material.emissiveColor = glm::vec3(0.25f, 0.55f, 0.95f);
            pillar.material.emissiveStrength = 0.6f;
            pillar.material.patternType = 3;

            pillar.collider.enabled = true;
            pillar.collider.isSphere = true;
            pillar.collider.sphereRadius = 1.35f;
            pillar.rigidBody.enabled = true;
            pillar.rigidBody.isKinematic = true;
        }

        // 9. Collectible Energy Crystals around the arena
        glm::vec3 crystalCoords[6] = {
            { -9.5f, 0.95f, -4.5f },
            {  9.5f, 0.95f, -4.5f },
            { -18.0f, 0.95f, 4.0f },
            {  18.0f, 0.95f, 4.0f },
            { -8.0f, 0.95f, 18.0f },
            {  8.0f, 0.95f, 18.0f }
        };
        for (int i = 0; i < 6; ++i)
        {
            SpawnEnergyCrystal(crystalCoords[i], (i % 2 == 1));
        }

        // 10. Wave 1 Enemy Seeker Drones
        glm::vec3 droneSpawns[5] = {
            { -18.0f, 0.9f, 24.0f },
            {  -8.0f, 0.9f, 26.0f },
            {   0.0f, 0.9f, 28.0f },
            {   8.0f, 0.9f, 26.0f },
            {  18.0f, 0.9f, 24.0f }
        };
        for (int i = 0; i < 5; ++i)
        {
            SpawnEnemyDrone(droneSpawns[i], 1);
        }
        enemiesRemainingInWave = 5;

        Log::Info("Built 'Starforge Outpost: Crystal Defense' scene with " + std::to_string(m_Entities.size()) + " entities.");
    }
}
