#include "SceneTemplates.h"
#include "../core/Log.h"
#include <algorithm>
#include <cmath>

namespace Hargon
{
    Entity& SceneTemplates::SpawnEnemyDrone(Scene& scene, const glm::vec3& pos, int waveLevel)
    {
        int idx = static_cast<int>(scene.FindEntitiesByTag("Enemy").size()) + 1;
        Entity& drone = scene.CreateEntity("SeekerDrone", "Enemy");
        drone.tag.name = "SeekerDrone_" + std::to_string(drone.id);
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

    Entity& SceneTemplates::SpawnEnergyCrystal(Scene& scene, const glm::vec3& pos, bool golden)
    {
        Entity& crystal = scene.CreateEntity(golden ? "SolarCrystal" : "EnergyCrystal", "Crystal");
        crystal.tag.name = std::string(golden ? "SolarCrystal_" : "EnergyCrystal_") + std::to_string(crystal.id);
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
        crystal.gameplay.health = golden ? 35.0f : 20.0f;

        return crystal;
    }

    Entity& SceneTemplates::SpawnProjectile(
        Scene& scene,
        const glm::vec3& pos,
        const glm::vec3& dir,
        EntityID ownerId,
        bool isEnemyProjectile,
        float speed,
        float damage)
    {
        Entity& proj = scene.CreateEntity(
            isEnemyProjectile ? "EnemyPlasmaBolt" : "PlayerPlasmaBolt",
            isEnemyProjectile ? "EnemyProjectile" : "Projectile");
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

    void SceneTemplates::BuildStarforgeOutpostScene(Scene& scene)
    {
        scene.Clear();
        scene.sceneName = "Starforge Outpost: Crystal Defense";
        scene.maxWaves = 3;
        scene.introBannerText = "WAVE 1: DEFEND THE STARFORGE CORE!";
        scene.hudBannerText = scene.introBannerText;
        scene.hudHealthLabel = "ROVER HULL";
        scene.hudShieldLabel = "ENERGY SHIELD";
        scene.hudObjectiveLabel = "STARFORGE CORE";
        scene.hudCollectibleLabel = "CRYSTALS";
        scene.hudControlsText = "[W/S] Thrust   [A/D] Steer   [SPACE] Fire Plasma Cannons   [SHIFT] Boost   [R] Restart";
        scene.victoryText = "VICTORY! STARFORGE OUTPOST SECURED!";
        scene.defeatText = "OUTPOST OVERRUN! PRESS [R] TO RESTART MISSION";

        {
            Entity& sun = scene.CreateEntity("SolarDirectionalLight", "Light");
            sun.transform.position = glm::vec3(20.0f, 35.0f, -15.0f);
            sun.transform.rotation = glm::vec3(48.0f, -35.0f, 0.0f);
            sun.mesh.enabled = false;
            sun.light.enabled = true;
            sun.light.isDirectional = true;
            sun.light.color = glm::vec3(1.0f, 0.95f, 0.86f);
            sun.light.intensity = 2.6f;
        }

        {
            Entity& cam = scene.CreateEntity("MainGameCamera", "Camera");
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

        {
            Entity& ground = scene.CreateEntity("ArenaGround", "Environment");
            ground.transform.position = glm::vec3(0.0f, -0.5f, 0.0f);
            ground.transform.scale = glm::vec3(76.0f, 1.0f, 76.0f);
            ground.mesh.enabled = true;
            ground.mesh.primitive = MeshPrimitive::Cube;
            ground.material.albedoColor = glm::vec4(0.11f, 0.14f, 0.20f, 1.0f);
            ground.material.metallic = 0.35f;
            ground.material.roughness = 0.55f;
            ground.material.patternType = 1;
            ground.material.uvScale = 1.0f;
        }

        struct WallDef { std::string name; glm::vec3 pos; glm::vec3 scale; };
        WallDef walls[4] = {
            { "NorthWall", {  0.0f, 1.2f,  37.0f }, { 76.0f, 2.8f, 1.5f } },
            { "SouthWall", {  0.0f, 1.2f, -37.0f }, { 76.0f, 2.8f, 1.5f } },
            { "EastWall",  {  37.0f, 1.2f,  0.0f }, { 1.5f,  2.8f, 76.0f } },
            { "WestWall",  { -37.0f, 1.2f,  0.0f }, { 1.5f,  2.8f, 76.0f } }
        };
        for (const auto& w : walls)
        {
            Entity& wall = scene.CreateEntity(w.name, "Environment");
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

        {
            Entity& core = scene.CreateEntity("StarforgeCore", "Core");
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

        {
            Entity& player = scene.CreateEntity("PlayerRover", "Player");
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

        glm::vec3 turretPositions[2] = {
            { -6.5f, 0.85f,  4.0f },
            {  6.5f, 0.85f,  4.0f }
        };
        for (int i = 0; i < 2; ++i)
        {
            Entity& turret = scene.CreateEntity("DefenseTurret_" + std::to_string(i + 1), "Turret");
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

        glm::vec3 pillarPositions[4] = {
            { -14.0f, 1.8f, -12.0f },
            {  14.0f, 1.8f, -12.0f },
            { -14.0f, 1.8f,  14.0f },
            {  14.0f, 1.8f,  14.0f }
        };
        for (int i = 0; i < 4; ++i)
        {
            Entity& pillar = scene.CreateEntity("EnergyPylon_" + std::to_string(i + 1), "Obstacle");
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

        glm::vec3 crystalCoords[6] = {
            { -9.5f, 0.95f, -4.5f },
            {  9.5f, 0.95f, -4.5f },
            { -18.0f, 0.95f, 4.0f },
            {  18.0f, 0.95f, 4.0f },
            { -8.0f, 0.95f, 18.0f },
            {  8.0f, 0.95f, 18.0f }
        };
        for (int i = 0; i < 6; ++i)
            SpawnEnergyCrystal(scene, crystalCoords[i], (i % 2 == 1));

        glm::vec3 droneSpawns[5] = {
            { -18.0f, 0.9f, 24.0f },
            {  -8.0f, 0.9f, 26.0f },
            {   0.0f, 0.9f, 28.0f },
            {   8.0f, 0.9f, 26.0f },
            {  18.0f, 0.9f, 24.0f }
        };
        for (int i = 0; i < 5; ++i)
            SpawnEnemyDrone(scene, droneSpawns[i], 1);
        scene.enemiesRemainingInWave = 5;

        Log::Info("Built 'Starforge Outpost: Crystal Defense' scene with " + std::to_string(scene.GetEntities().size()) + " entities.");
    }

    void SceneTemplates::BuildAetherRealmsScene(Scene& scene)
    {
        scene.Clear();
        scene.sceneName = "Aether Realms: Skyward Sanctum";
        scene.skyZenithColor = glm::vec3(0.06f, 0.24f, 0.58f);
        scene.skyHorizonColor = glm::vec3(0.55f, 0.78f, 0.96f);
        scene.groundAmbientColor = glm::vec3(0.24f, 0.34f, 0.42f);
        scene.ambientIntensity = 0.68f;
        scene.fogDensity = 0.0022f;
        scene.arenaHalfSize = 48.0f;
        scene.maxWaves = 1;

        scene.worldEnv.tonemapMode = ToneMapperMode::AgX;
        scene.worldEnv.tonemapExposure = 1.12f;
        scene.worldEnv.glowEnabled = true;
        scene.worldEnv.glowIntensity = 0.42f;
        scene.worldEnv.glowBloom = 0.18f;
        scene.worldEnv.fogHeight = -6.0f;
        scene.worldEnv.fogHeightDensity = 0.08f;
        scene.worldEnv.fogSunScatter = 0.35f;
        scene.worldEnv.adjustmentEnabled = true;
        scene.worldEnv.adjustmentBrightness = 1.04f;
        scene.worldEnv.adjustmentContrast = 1.06f;
        scene.worldEnv.adjustmentSaturation = 1.16f;

        scene.introBannerText = "COLLECT ALL 8 SKYWARD RELICS & DEFEAT THE SKY-CONSTRUCT GUARDIANS!";
        scene.hudBannerText = scene.introBannerText;
        scene.hudHealthLabel = "EXPLORER VITALITY";
        scene.hudShieldLabel = "AETHER BARRIER";
        scene.hudObjectiveLabel = "SANCTUM SPIRE";
        scene.hudCollectibleLabel = "RELICS";
        scene.hudControlsText = "[W/S] Run   [A/D] Turn   [SPACE] Cast Aether Bolt   [SHIFT] Dash   [R] Restart";
        scene.victoryText = "THE SANCTUM SPIRE AWAKENS! REALM RESTORED!";
        scene.defeatText = "YOU FELL FROM THE SKY... PRESS [R] TO TRY AGAIN";
        scene.hudBannerTimer = 5.0f;

        {
            Entity& sun = scene.CreateEntity("CelestialSunLight", "Light");
            sun.transform.position = glm::vec3(0.0f, 35.0f, -20.0f);
            sun.transform.rotation = glm::vec3(48.0f, -32.0f, 0.0f);
            sun.mesh.enabled = false;
            sun.light.enabled = true;
            sun.light.isDirectional = true;
            sun.light.color = glm::vec3(1.0f, 0.96f, 0.84f);
            sun.light.intensity = 3.2f;
        }

        {
            Entity& cam = scene.CreateEntity("ThirdPersonOrbitCamera", "Camera");
            cam.transform.position = glm::vec3(0.0f, 9.5f, -22.0f);
            cam.mesh.enabled = false;
            cam.camera.enabled = true;
            cam.camera.isPrimary = true;
            cam.camera.mode = CameraMode::ThirdPersonOrbit;
            cam.camera.fov = 62.0f;
            cam.camera.followTargetName = "SkyExplorer";
            cam.camera.followOffset = glm::vec3(0.0f, 8.5f, -13.0f);
            cam.camera.followLookOffset = glm::vec3(0.0f, 1.4f, 3.0f);
            cam.camera.springArmLength = 13.5f;
            cam.camera.orbitPitch = 24.0f;
            cam.camera.smoothSpeed = 9.0f;
        }

        {
            Entity& cloudSea = scene.CreateEntity("LowerCloudSea", "Environment");
            cloudSea.transform.position = glm::vec3(0.0f, -6.5f, 0.0f);
            cloudSea.transform.scale = glm::vec3(140.0f, 1.0f, 140.0f);
            cloudSea.mesh.enabled = true;
            cloudSea.mesh.primitive = MeshPrimitive::Plane;
            cloudSea.material.albedoColor = glm::vec4(0.62f, 0.82f, 0.98f, 1.0f);
            cloudSea.material.metallic = 0.08f;
            cloudSea.material.roughness = 0.65f;
            cloudSea.material.uvScale = 18.0f;
            cloudSea.material.patternType = 2;
        }

        struct IslandDef
        {
            const char* name;
            glm::vec3 pos;
            glm::vec3 scale;
            glm::vec4 color;
            int pattern;
            bool animatedBob;
        };
        IslandDef islands[] = {
            { "SanctumMainPlaza",   {  0.0f, -0.6f,   0.0f }, { 26.0f, 1.4f, 26.0f }, { 0.32f, 0.72f, 0.44f, 1.0f }, 6, false },
            { "SouthArrivalIsle",   {  0.0f, -0.6f, -20.0f }, { 14.0f, 1.4f, 12.0f }, { 0.36f, 0.76f, 0.48f, 1.0f }, 6, false },
            { "NorthSpireCitadel",  {  0.0f, -0.5f,  23.0f }, { 18.0f, 1.6f, 16.0f }, { 0.82f, 0.78f, 0.66f, 1.0f }, 5, false },
            { "WestWindTerrace",    {-23.0f, -0.5f,   2.0f }, { 14.0f, 1.4f, 16.0f }, { 0.40f, 0.74f, 0.52f, 1.0f }, 6, true  },
            { "EastCrystalGarden",  { 23.0f, -0.5f,   2.0f }, { 14.0f, 1.4f, 16.0f }, { 0.40f, 0.74f, 0.52f, 1.0f }, 6, true  },
            { "SkyBridge_South",    {  0.0f, -0.45f,-10.5f }, {  4.8f, 0.6f,  8.0f }, { 0.78f, 0.72f, 0.58f, 1.0f }, 5, false },
            { "SkyBridge_North",    {  0.0f, -0.45f, 12.0f }, {  4.8f, 0.6f,  8.0f }, { 0.78f, 0.72f, 0.58f, 1.0f }, 5, false },
            { "SkyBridge_West",     {-13.0f, -0.45f,  1.5f }, {  8.0f, 0.6f,  4.5f }, { 0.78f, 0.72f, 0.58f, 1.0f }, 5, false },
            { "SkyBridge_East",     { 13.0f, -0.45f,  1.5f }, {  8.0f, 0.6f,  4.5f }, { 0.78f, 0.72f, 0.58f, 1.0f }, 5, false }
        };
        for (const auto& isl : islands)
        {
            Entity& plat = scene.CreateEntity(isl.name, "Environment");
            plat.transform.position = isl.pos;
            plat.transform.scale = isl.scale;
            plat.mesh.enabled = true;
            plat.mesh.primitive = MeshPrimitive::Cube;
            plat.material.albedoColor = isl.color;
            plat.material.metallic = 0.15f;
            plat.material.roughness = 0.52f;
            plat.material.uvScale = 4.0f;
            plat.material.patternType = isl.pattern;
            if (isl.animatedBob)
            {
                plat.animation.enabled = true;
                plat.animation.playing = true;
                plat.animation.animName = "IslandFloat";
                plat.animation.trackType = 0;
                plat.animation.speed = 1.1f;
                plat.animation.amplitude = 0.35f;
            }
        }

        {
            Entity& spire = scene.CreateEntity("SanctumSpire", "Core");
            spire.transform.position = glm::vec3(0.0f, 2.4f, 23.0f);
            spire.transform.scale = glm::vec3(2.5f, 4.6f, 2.5f);
            spire.mesh.enabled = true;
            spire.mesh.primitive = MeshPrimitive::PropObelisk;
            spire.material.albedoColor = glm::vec4(0.95f, 0.85f, 0.42f, 1.0f);
            spire.material.metallic = 0.72f;
            spire.material.roughness = 0.20f;
            spire.material.emissiveColor = glm::vec3(1.0f, 0.82f, 0.30f);
            spire.material.emissiveStrength = 1.8f;

            spire.light.enabled = true;
            spire.light.color = glm::vec3(1.0f, 0.86f, 0.42f);
            spire.light.intensity = 5.0f;
            spire.light.range = 24.0f;

            spire.particleEmitter.enabled = true;
            spire.particleEmitter.emitting = true;
            spire.particleEmitter.emissionShape = 3;
            spire.particleEmitter.emissionRadius = 2.4f;
            spire.particleEmitter.emissionRate = 28.0f;
            spire.particleEmitter.lifetime = 0.9f;
            spire.particleEmitter.initialSpeed = 5.5f;
            spire.particleEmitter.startColor = glm::vec3(1.0f, 0.88f, 0.32f);
            spire.particleEmitter.endColor = glm::vec3(0.25f, 0.85f, 1.0f);
            spire.particleEmitter.particleSize = 0.42f;

            spire.animation.enabled = true;
            spire.animation.playing = true;
            spire.animation.animName = "SpireSpin";
            spire.animation.trackType = 1;
            spire.animation.speed = 32.0f;

            spire.gameplay.enabled = true;
            spire.gameplay.maxHealth = 500.0f;
            spire.gameplay.health = 500.0f;
        }

        glm::vec3 millCoords[2] = {
            { -23.0f, 2.5f,  6.0f },
            {  23.0f, 2.5f,  6.0f }
        };
        for (int i = 0; i < 2; ++i)
        {
            Entity& tower = scene.CreateEntity("AetherWindTower_" + std::to_string(i + 1), "Obstacle");
            tower.transform.position = millCoords[i];
            tower.transform.scale = glm::vec3(1.8f, 5.0f, 1.8f);
            tower.mesh.enabled = true;
            tower.mesh.primitive = MeshPrimitive::PropPillar;
            tower.material.albedoColor = glm::vec4(0.86f, 0.82f, 0.72f, 1.0f);
            tower.material.roughness = 0.42f;
            tower.material.patternType = 5;
            EntityID towerId = tower.id;

            Entity& rotor = scene.CreateEntity("WindRotorBlades_" + std::to_string(i + 1), "Decoration");
            rotor.transform.position = glm::vec3(0.0f, 0.55f, 0.65f);
            rotor.transform.scale = glm::vec3(2.8f, 0.35f, 0.35f);
            rotor.mesh.enabled = true;
            rotor.mesh.primitive = MeshPrimitive::Cube;
            rotor.material.albedoColor = glm::vec4(0.30f, 0.88f, 1.0f, 1.0f);
            rotor.material.emissiveColor = glm::vec3(0.20f, 0.80f, 1.0f);
            rotor.material.emissiveStrength = 1.4f;
            rotor.animation.enabled = true;
            rotor.animation.playing = true;
            rotor.animation.animName = "RotorSpin";
            rotor.animation.trackType = 1;
            rotor.animation.axis = glm::vec3(0.0f, 0.0f, 1.0f);
            rotor.animation.speed = 95.0f;
            scene.SetParent(rotor.id, towerId);
        }

        {
            Entity& hero = scene.CreateEntity("SkyExplorer", "Player");
            hero.transform.position = glm::vec3(0.0f, 0.95f, -18.0f);
            hero.transform.scale = glm::vec3(1.25f, 1.45f, 1.25f);
            hero.mesh.enabled = true;
            hero.mesh.primitive = MeshPrimitive::EnemyKnight;
            hero.material.albedoColor = glm::vec4(0.95f, 0.78f, 0.25f, 1.0f);
            hero.material.metallic = 0.78f;
            hero.material.roughness = 0.20f;
            hero.material.emissiveColor = glm::vec3(0.25f, 0.85f, 1.0f);
            hero.material.emissiveStrength = 0.85f;

            hero.light.enabled = true;
            hero.light.color = glm::vec3(0.35f, 0.88f, 1.0f);
            hero.light.intensity = 2.6f;
            hero.light.range = 12.0f;

            hero.rigidBody.enabled = true;
            hero.rigidBody.mass = 1.2f;
            hero.rigidBody.drag = 6.0f;
            hero.rigidBody.freezeY = true;

            hero.collider.enabled = true;
            hero.collider.isSphere = true;
            hero.collider.sphereRadius = 1.15f;

            hero.particleEmitter.enabled = true;
            hero.particleEmitter.emitting = true;
            hero.particleEmitter.emissionShape = 0;
            hero.particleEmitter.emissionRate = 14.0f;
            hero.particleEmitter.lifetime = 0.45f;
            hero.particleEmitter.initialSpeed = 2.2f;
            hero.particleEmitter.startColor = glm::vec3(0.35f, 0.92f, 1.0f);
            hero.particleEmitter.endColor = glm::vec3(0.95f, 0.80f, 0.25f);
            hero.particleEmitter.particleSize = 0.28f;

            hero.luaScript.enabled = true;
            hero.luaScript.scriptPath = "assets/scripts/player_rover.lua";

            hero.gameplay.enabled = true;
            hero.gameplay.maxHealth = 120.0f;
            hero.gameplay.health = 120.0f;
            hero.gameplay.maxShield = 100.0f;
            hero.gameplay.shield = 100.0f;
            hero.gameplay.moveSpeed = 17.5f;
            hero.gameplay.fireRate = 0.16f;
            hero.gameplay.damage = 30.0f;
        }

        glm::vec3 relicPos[8] = {
            {  0.0f, 1.1f, -10.0f },
            { -7.5f, 1.1f,  -4.0f },
            {  7.5f, 1.1f,  -4.0f },
            {  0.0f, 1.1f,   3.0f },
            {-22.0f, 1.2f,   1.0f },
            { 22.0f, 1.2f,   1.0f },
            { -5.5f, 1.2f,  19.0f },
            {  5.5f, 1.2f,  19.0f }
        };
        for (int i = 0; i < 8; ++i)
        {
            Entity& c = SpawnEnergyCrystal(scene, relicPos[i], true);
            c.tag.name = "SkywardRelic_" + std::to_string(i + 1);
            c.animation.enabled = true;
            c.animation.playing = true;
            c.animation.animName = "RelicHoverSpin";
            c.animation.trackType = 0;
            c.animation.speed = 2.4f;
            c.animation.amplitude = 0.35f;
            c.particleEmitter.enabled = true;
            c.particleEmitter.emitting = true;
            c.particleEmitter.emissionRate = 10.0f;
            c.particleEmitter.lifetime = 0.6f;
            c.particleEmitter.initialSpeed = 2.5f;
            c.particleEmitter.startColor = glm::vec3(1.0f, 0.90f, 0.25f);
            c.particleEmitter.endColor = glm::vec3(0.25f, 0.95f, 1.0f);
        }

        glm::vec3 guardianPos[4] = {
            { -8.0f, 1.1f,   8.0f },
            {  8.0f, 1.1f,   8.0f },
            {-20.0f, 1.1f,  -3.0f },
            { 20.0f, 1.1f,  -3.0f }
        };
        for (int i = 0; i < 4; ++i)
        {
            Entity& g = SpawnEnemyDrone(scene, guardianPos[i], 1);
            g.tag.name = "SkyConstructGuardian_" + std::to_string(i + 1);
            g.mesh.primitive = MeshPrimitive::EnemyScrag;
            g.material.albedoColor = glm::vec4(0.30f, 0.88f, 0.95f, 1.0f);
            g.material.emissiveColor = glm::vec3(0.20f, 0.75f, 1.0f);
            g.material.emissiveStrength = 1.4f;
        }
        scene.enemiesRemainingInWave = 4;

        Log::Info("Built 'Aether Realms: Skyward Sanctum' 3D platformer-adventure scene with " + std::to_string(scene.GetEntities().size()) + " entities.");
    }

    void SceneTemplates::BuildSkywayCourseScene(Scene& scene, const std::string& title)
    {
        scene.Clear();
        scene.sceneName = title.empty() ? "Skyway 3D Time-Trial" : title;
        scene.skyZenithColor = glm::vec3(0.08f, 0.16f, 0.42f);
        scene.skyHorizonColor = glm::vec3(0.95f, 0.62f, 0.42f);
        scene.groundAmbientColor = glm::vec3(0.22f, 0.24f, 0.30f);
        scene.ambientIntensity = 0.68f;
        scene.fogDensity = 0.0018f;
        scene.arenaHalfSize = 60.0f;
        scene.maxWaves = 1;

        scene.worldEnv.tonemapMode = ToneMapperMode::AgX;
        scene.worldEnv.tonemapExposure = 1.10f;
        scene.worldEnv.glowEnabled = true;
        scene.worldEnv.glowIntensity = 0.45f;
        scene.worldEnv.glowBloom = 0.22f;
        scene.worldEnv.fogSunScatter = 0.40f;

        scene.introBannerText = "COLLECT ALL 6 SOLAR BEACONS ACROSS THE OBSTACLE COURSE!";
        scene.hudBannerText = scene.introBannerText;
        scene.hudHealthLabel = "STAMINA";
        scene.hudShieldLabel = "BOOST METER";
        scene.hudObjectiveLabel = "FINISH ARCH";
        scene.hudCollectibleLabel = "BEACONS";
        scene.hudControlsText = "[W/S] Move   [A/D] Steer   [SHIFT] Turbo Boost   [SPACE] Pulse   [R] Restart";
        scene.victoryText = "COURSE COMPLETE! ALL SKYWAY BEACONS COLLECTED!";
        scene.defeatText = "FELL OFF COURSE! PRESS [R] TO RETRY";
        scene.hudBannerTimer = 4.5f;

        {
            Entity& sun = scene.CreateEntity("SunsetSun", "Light");
            sun.transform.position = glm::vec3(15.0f, 32.0f, -25.0f);
            sun.transform.rotation = glm::vec3(38.0f, -25.0f, 0.0f);
            sun.mesh.enabled = false;
            sun.light.enabled = true;
            sun.light.isDirectional = true;
            sun.light.color = glm::vec3(1.0f, 0.88f, 0.72f);
            sun.light.intensity = 3.1f;
        }

        {
            Entity& cam = scene.CreateEntity("ChaseOrbitCam", "Camera");
            cam.transform.position = glm::vec3(0.0f, 8.0f, -28.0f);
            cam.mesh.enabled = false;
            cam.camera.enabled = true;
            cam.camera.isPrimary = true;
            cam.camera.mode = CameraMode::ThirdPersonOrbit;
            cam.camera.fov = 64.0f;
            cam.camera.followOffset = glm::vec3(0.0f, 7.5f, -12.0f);
            cam.camera.followLookOffset = glm::vec3(0.0f, 1.5f, 4.0f);
            cam.camera.springArmLength = 12.0f;
            cam.camera.orbitPitch = 21.0f;
            cam.camera.smoothSpeed = 10.0f;
        }

        // Long sunlit skyway course segments
        struct TrackSeg { const char* name; glm::vec3 pos; glm::vec3 scale; glm::vec4 col; int pat; };
        TrackSeg segs[] = {
            { "CourseDeck_Start",  {  0.0f, -0.5f, -12.0f }, { 14.0f, 1.0f, 22.0f }, { 0.22f, 0.30f, 0.44f, 1.0f }, 1 },
            { "CourseDeck_Mid",    {  0.0f, -0.5f,  12.0f }, { 18.0f, 1.0f, 26.0f }, { 0.25f, 0.34f, 0.48f, 1.0f }, 1 },
            { "CourseDeck_Finish", {  0.0f, -0.5f,  36.0f }, { 16.0f, 1.0f, 18.0f }, { 0.28f, 0.42f, 0.46f, 1.0f }, 2 }
        };
        for (const auto& seg : segs)
        {
            Entity& deck = scene.CreateEntity(seg.name, "Environment");
            deck.transform.position = seg.pos;
            deck.transform.scale = seg.scale;
            deck.mesh.enabled = true;
            deck.mesh.primitive = MeshPrimitive::Cube;
            deck.material.albedoColor = seg.col;
            deck.material.metallic = 0.25f;
            deck.material.roughness = 0.45f;
            deck.material.patternType = seg.pat;
            deck.material.uvScale = 2.0f;
            deck.collider.enabled = true;
            deck.collider.isSphere = false;
            deck.collider.boxHalfExtents = seg.scale * 0.5f;
            deck.rigidBody.enabled = true;
            deck.rigidBody.isKinematic = true;
        }

        // Animated sweeping barrier pylons
        glm::vec3 obsCoords[3] = {
            { -2.5f, 1.2f,  2.0f },
            {  2.5f, 1.2f, 14.0f },
            {  0.0f, 1.2f, 25.0f }
        };
        for (int i = 0; i < 3; ++i)
        {
            Entity& obs = scene.CreateEntity("MovingBumper_" + std::to_string(i + 1), "Obstacle");
            obs.transform.position = obsCoords[i];
            obs.transform.scale = glm::vec3(2.2f, 2.4f, 2.2f);
            obs.mesh.enabled = true;
            obs.mesh.primitive = MeshPrimitive::Cylinder;
            obs.material.albedoColor = glm::vec4(0.95f, 0.36f, 0.22f, 1.0f);
            obs.material.emissiveColor = glm::vec3(1.0f, 0.28f, 0.12f);
            obs.material.emissiveStrength = 1.3f;
            obs.collider.enabled = true;
            obs.collider.isSphere = true;
            obs.collider.sphereRadius = 1.2f;
            obs.rigidBody.enabled = true;
            obs.rigidBody.isKinematic = true;
            obs.animation.enabled = true;
            obs.animation.playing = true;
            obs.animation.animName = "BumperSweep";
            obs.animation.trackType = 2; // PatrolPingPong
            obs.animation.axis = glm::vec3(1.0f, 0.0f, 0.0f);
            obs.animation.speed = 1.8f + i * 0.3f;
            obs.animation.amplitude = 1.2f;
        }

        // Player Runner
        {
            Entity& runner = scene.CreateEntity("RunnerCraft", "Player");
            runner.transform.position = glm::vec3(0.0f, 0.8f, -18.0f);
            runner.transform.scale = glm::vec3(1.25f);
            runner.mesh.enabled = true;
            runner.mesh.primitive = MeshPrimitive::Rover;
            runner.material.albedoColor = glm::vec4(1.0f, 0.65f, 0.18f, 1.0f);
            runner.material.metallic = 0.75f;
            runner.material.roughness = 0.20f;
            runner.material.emissiveColor = glm::vec3(1.0f, 0.55f, 0.15f);
            runner.material.emissiveStrength = 0.9f;
            runner.light.enabled = true;
            runner.light.color = glm::vec3(1.0f, 0.72f, 0.30f);
            runner.light.intensity = 2.4f;
            runner.light.range = 10.0f;
            runner.rigidBody.enabled = true;
            runner.rigidBody.mass = 1.2f;
            runner.rigidBody.drag = 5.5f;
            runner.rigidBody.freezeY = true;
            runner.collider.enabled = true;
            runner.collider.isSphere = true;
            runner.collider.sphereRadius = 1.1f;
            runner.luaScript.enabled = true;
            runner.luaScript.scriptPath = "assets/scripts/player_rover.lua";
            runner.gameplay.enabled = true;
            runner.gameplay.maxHealth = 100.0f;
            runner.gameplay.health = 100.0f;
            runner.gameplay.maxShield = 100.0f;
            runner.gameplay.shield = 100.0f;
            runner.gameplay.moveSpeed = 19.5f;
        }

        // 6 Solar Beacons along the track
        glm::vec3 beaconCoords[6] = {
            {  0.0f, 1.1f, -8.0f },
            { -3.8f, 1.1f,  0.0f },
            {  3.8f, 1.1f,  8.0f },
            { -3.5f, 1.1f, 18.0f },
            {  3.5f, 1.1f, 28.0f },
            {  0.0f, 1.2f, 38.0f }
        };
        for (int i = 0; i < 6; ++i)
        {
            Entity& b = SpawnEnergyCrystal(scene, beaconCoords[i], true);
            b.tag.name = "SolarBeacon_" + std::to_string(i + 1);
            b.animation.enabled = true;
            b.animation.playing = true;
            b.animation.trackType = 0;
            b.animation.speed = 2.6f;
            b.animation.amplitude = 0.30f;
        }
        scene.enemiesRemainingInWave = 0;

        Log::Info("Built 'Skyway 3D Time-Trial' obstacle course scene with " + std::to_string(scene.GetEntities().size()) + " entities.");
    }

    void SceneTemplates::BuildEmptyTemplateScene(Scene& scene, const std::string& title)
    {
        scene.Clear();
        scene.sceneName = title;
        scene.maxWaves = 1;
        scene.introBannerText = "SANDBOX: " + title;
        scene.hudBannerText = scene.introBannerText;
        scene.hudHealthLabel = "HEALTH";
        scene.hudShieldLabel = "ENERGY";
        scene.hudObjectiveLabel = "SANDBOX";
        scene.hudCollectibleLabel = "PICKUPS";
        scene.hudControlsText = "[W/S] Move   [A/D] Turn   [SPACE] Action   [SHIFT] Boost   [R] Restart";
        scene.victoryText = "OBJECTIVE COMPLETE!";
        scene.defeatText = "DEFEATED! PRESS [R] TO RESTART";
        scene.skyZenithColor = glm::vec3(0.05f, 0.14f, 0.34f);
        scene.skyHorizonColor = glm::vec3(0.32f, 0.56f, 0.78f);
        scene.groundAmbientColor = glm::vec3(0.15f, 0.18f, 0.22f);
        scene.ambientIntensity = 0.60f;
        scene.fogDensity = 0.0025f;

        {
            Entity& sun = scene.CreateEntity("DirectionalSun", "Light");
            sun.transform.position = glm::vec3(0.0f, 25.0f, -15.0f);
            sun.transform.rotation = glm::vec3(45.0f, -30.0f, 0.0f);
            sun.mesh.enabled = false;
            sun.light.enabled = true;
            sun.light.isDirectional = true;
            sun.light.color = glm::vec3(1.0f, 0.96f, 0.88f);
            sun.light.intensity = 2.8f;
        }

        {
            Entity& cam = scene.CreateEntity("MainCamera", "Camera");
            cam.transform.position = glm::vec3(0.0f, 8.0f, -14.0f);
            cam.mesh.enabled = false;
            cam.camera.enabled = true;
            cam.camera.isPrimary = true;
            cam.camera.fov = 60.0f;
            cam.camera.followTargetName = "Player";
        }

        {
            Entity& floor = scene.CreateEntity("GroundPlane", "Environment");
            floor.transform.position = glm::vec3(0.0f, -0.5f, 0.0f);
            floor.transform.scale = glm::vec3(40.0f, 1.0f, 40.0f);
            floor.mesh.enabled = true;
            floor.mesh.primitive = MeshPrimitive::Cube;
            floor.material.albedoColor = glm::vec4(0.28f, 0.34f, 0.42f, 1.0f);
            floor.material.patternType = 2;
            floor.material.uvScale = 10.0f;
        }

        {
            Entity& hero = scene.CreateEntity("Player", "Player");
            hero.transform.position = glm::vec3(0.0f, 0.8f, 0.0f);
            hero.mesh.enabled = true;
            hero.mesh.primitive = MeshPrimitive::Rover;
            hero.material.albedoColor = glm::vec4(0.25f, 0.78f, 0.98f, 1.0f);
            hero.rigidBody.enabled = true;
            hero.collider.enabled = true;
            hero.luaScript.enabled = true;
            hero.luaScript.scriptPath = "assets/scripts/player_rover.lua";
            hero.gameplay.enabled = true;
        }
    }
}
