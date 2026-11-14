#include "SceneSerializer.h"
#include "../core/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace Hargon
{
    using json = nlohmann::json;

    static json Vec3ToJson(const glm::vec3& v)
    {
        return json::array({ v.x, v.y, v.z });
    }

    static glm::vec3 JsonToVec3(const json& j, const glm::vec3& def = glm::vec3(0.0f))
    {
        if (j.is_array() && j.size() >= 3)
            return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
        return def;
    }

    static json Vec4ToJson(const glm::vec4& v)
    {
        return json::array({ v.x, v.y, v.z, v.w });
    }

    static glm::vec4 JsonToVec4(const json& j, const glm::vec4& def = glm::vec4(1.0f))
    {
        if (j.is_array() && j.size() >= 4)
            return glm::vec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
        return def;
    }

    std::string SceneSerializer::SerializeToString(const Scene& scene)
    {
        json root;
        root["sceneName"] = scene.sceneName;
        root["skyZenithColor"] = Vec3ToJson(scene.skyZenithColor);
        root["skyHorizonColor"] = Vec3ToJson(scene.skyHorizonColor);
        root["groundAmbientColor"] = Vec3ToJson(scene.groundAmbientColor);
        root["ambientIntensity"] = scene.ambientIntensity;
        root["fogDensity"] = scene.fogDensity;
        root["arenaHalfSize"] = scene.arenaHalfSize;
        root["maxWaves"] = scene.maxWaves;

        root["worldEnv"] = {
            {"tonemapMode",          static_cast<int>(scene.worldEnv.tonemapMode)},
            {"tonemapExposure",      scene.worldEnv.tonemapExposure},
            {"tonemapWhite",         scene.worldEnv.tonemapWhite},
            {"glowEnabled",          scene.worldEnv.glowEnabled},
            {"glowIntensity",        scene.worldEnv.glowIntensity},
            {"glowBloom",            scene.worldEnv.glowBloom},
            {"fogHeight",            scene.worldEnv.fogHeight},
            {"fogHeightDensity",     scene.worldEnv.fogHeightDensity},
            {"fogSunScatter",        scene.worldEnv.fogSunScatter},
            {"adjustmentEnabled",    scene.worldEnv.adjustmentEnabled},
            {"adjustmentBrightness", scene.worldEnv.adjustmentBrightness},
            {"adjustmentContrast",   scene.worldEnv.adjustmentContrast},
            {"adjustmentSaturation", scene.worldEnv.adjustmentSaturation},
            {"frustumCullingEnabled",scene.worldEnv.frustumCullingEnabled}
        };

        json entitiesArr = json::array();
        for (const auto& e : scene.GetEntities())
        {
            if (!e.alive) continue;

            json ej;
            ej["id"] = e.id;
            ej["parentId"] = e.parentId;
            ej["processMode"] = static_cast<int>(e.processMode);
            ej["name"] = e.tag.name;
            ej["tag"] = e.tag.tag;
            ej["groups"] = e.tag.groups;

            ej["transform"] = {
                {"position", Vec3ToJson(e.transform.position)},
                {"rotation", Vec3ToJson(e.transform.rotation)},
                {"scale",    Vec3ToJson(e.transform.scale)}
            };

            ej["mesh"] = {
                {"enabled",       e.mesh.enabled},
                {"primitive",     MeshFactory::PrimitiveToString(e.mesh.primitive)},
                {"customObjPath", e.mesh.customObjPath}
            };

            ej["material"] = {
                {"albedoColor",      Vec4ToJson(e.material.albedoColor)},
                {"metallic",         e.material.metallic},
                {"roughness",        e.material.roughness},
                {"emissiveColor",    Vec3ToJson(e.material.emissiveColor)},
                {"emissiveStrength", e.material.emissiveStrength},
                {"uvScale",          e.material.uvScale},
                {"patternType",      e.material.patternType}
            };

            ej["light"] = {
                {"enabled",       e.light.enabled},
                {"isDirectional", e.light.isDirectional},
                {"color",         Vec3ToJson(e.light.color)},
                {"intensity",     e.light.intensity},
                {"range",         e.light.range}
            };

            ej["camera"] = {
                {"enabled",          e.camera.enabled},
                {"isPrimary",        e.camera.isPrimary},
                {"fov",              e.camera.fov},
                {"nearClip",         e.camera.nearClip},
                {"farClip",          e.camera.farClip},
                {"followTargetName", e.camera.followTargetName},
                {"followOffset",     Vec3ToJson(e.camera.followOffset)},
                {"followLookOffset", Vec3ToJson(e.camera.followLookOffset)},
                {"smoothSpeed",      e.camera.smoothSpeed}
            };

            ej["rigidBody"] = {
                {"enabled",     e.rigidBody.enabled},
                {"isKinematic", e.rigidBody.isKinematic},
                {"useGravity",  e.rigidBody.useGravity},
                {"freezeY",     e.rigidBody.freezeY},
                {"mass",        e.rigidBody.mass},
                {"drag",        e.rigidBody.drag},
                {"bounciness",  e.rigidBody.bounciness},
                {"velocity",    Vec3ToJson(e.rigidBody.velocity)}
            };

            ej["collider"] = {
                {"enabled",        e.collider.enabled},
                {"isSphere",       e.collider.isSphere},
                {"isTrigger",      e.collider.isTrigger},
                {"boxHalfExtents", Vec3ToJson(e.collider.boxHalfExtents)},
                {"sphereRadius",   e.collider.sphereRadius}
            };

            ej["audioSource"] = {
                {"enabled",     e.audioSource.enabled},
                {"soundName",   e.audioSource.soundName},
                {"volume",      e.audioSource.volume},
                {"pitch",       e.audioSource.pitch},
                {"loop",        e.audioSource.loop},
                {"spatial",     e.audioSource.spatial},
                {"playOnAwake", e.audioSource.playOnAwake}
            };

            ej["luaScript"] = {
                {"enabled",    e.luaScript.enabled},
                {"scriptPath", e.luaScript.scriptPath}
            };

            ej["gameplay"] = {
                {"enabled",      e.gameplay.enabled},
                {"health",       e.gameplay.health},
                {"maxHealth",    e.gameplay.maxHealth},
                {"shield",       e.gameplay.shield},
                {"maxShield",    e.gameplay.maxShield},
                {"moveSpeed",    e.gameplay.moveSpeed},
                {"fireRate",     e.gameplay.fireRate},
                {"fireCooldown", e.gameplay.fireCooldown},
                {"damage",       e.gameplay.damage},
                {"maxLifetime",  e.gameplay.maxLifetime},
                {"scoreValue",   e.gameplay.scoreValue},
                {"subType",      e.gameplay.subType},
                {"state",        e.gameplay.state},
                {"extraKey",     e.gameplay.extraKey},
                {"targetPos",    Vec3ToJson(e.gameplay.targetPos)},
                {"basePos",      Vec3ToJson(e.gameplay.basePos)}
            };

            entitiesArr.push_back(ej);
        }

        root["entities"] = entitiesArr;
        return root.dump(2);
    }

    bool SceneSerializer::DeserializeFromString(Scene& scene, const std::string& jsonText)
    {
        try
        {
            json root = json::parse(jsonText);
            scene.Clear();
            scene.sceneName = root.value("sceneName", "Starforge Outpost");
            scene.skyZenithColor = JsonToVec3(root["skyZenithColor"], scene.skyZenithColor);
            scene.skyHorizonColor = JsonToVec3(root["skyHorizonColor"], scene.skyHorizonColor);
            scene.groundAmbientColor = JsonToVec3(root["groundAmbientColor"], scene.groundAmbientColor);
            scene.ambientIntensity = root.value("ambientIntensity", 0.55f);
            scene.fogDensity = root.value("fogDensity", 0.0045f);
            scene.arenaHalfSize = root.value("arenaHalfSize", 36.0f);
            scene.maxWaves = root.value("maxWaves", 3);

            if (root.contains("worldEnv") && root["worldEnv"].is_object())
            {
                const auto& we = root["worldEnv"];
                scene.worldEnv.tonemapMode           = static_cast<ToneMapperMode>(we.value("tonemapMode", 4));
                scene.worldEnv.tonemapExposure       = we.value("tonemapExposure", 1.05f);
                scene.worldEnv.tonemapWhite          = we.value("tonemapWhite", 1.0f);
                scene.worldEnv.glowEnabled           = we.value("glowEnabled", true);
                scene.worldEnv.glowIntensity         = we.value("glowIntensity", 0.35f);
                scene.worldEnv.glowBloom             = we.value("glowBloom", 0.15f);
                scene.worldEnv.fogHeight             = we.value("fogHeight", 0.0f);
                scene.worldEnv.fogHeightDensity      = we.value("fogHeightDensity", 0.0f);
                scene.worldEnv.fogSunScatter         = we.value("fogSunScatter", 0.22f);
                scene.worldEnv.adjustmentEnabled     = we.value("adjustmentEnabled", true);
                scene.worldEnv.adjustmentBrightness  = we.value("adjustmentBrightness", 1.0f);
                scene.worldEnv.adjustmentContrast    = we.value("adjustmentContrast", 1.04f);
                scene.worldEnv.adjustmentSaturation  = we.value("adjustmentSaturation", 1.08f);
                scene.worldEnv.frustumCullingEnabled = we.value("frustumCullingEnabled", true);
            }

            if (root.contains("entities") && root["entities"].is_array())
            {
                for (const auto& ej : root["entities"])
                {
                    Entity& e = scene.CreateEntity(
                        ej.value("name", "Entity"),
                        ej.value("tag", "Default"));

                    e.processMode = static_cast<ProcessMode>(ej.value("processMode", 0));
                    if (ej.contains("groups") && ej["groups"].is_array())
                    {
                        for (const auto& g : ej["groups"])
                            e.tag.AddToGroup(g.get<std::string>());
                    }

                    if (ej.contains("transform"))
                    {
                        const auto& tj = ej["transform"];
                        e.transform.position = JsonToVec3(tj["position"], glm::vec3(0.0f));
                        e.transform.rotation = JsonToVec3(tj["rotation"], glm::vec3(0.0f));
                        e.transform.scale    = JsonToVec3(tj["scale"], glm::vec3(1.0f));
                    }

                    if (ej.contains("mesh"))
                    {
                        const auto& mj = ej["mesh"];
                        e.mesh.enabled = mj.value("enabled", true);
                        e.mesh.primitive = MeshFactory::StringToPrimitive(mj.value("primitive", "Cube"));
                        e.mesh.customObjPath = mj.value("customObjPath", "");
                    }

                    if (ej.contains("material"))
                    {
                        const auto& matj = ej["material"];
                        e.material.albedoColor = JsonToVec4(matj["albedoColor"], glm::vec4(0.85f));
                        e.material.metallic = matj.value("metallic", 0.25f);
                        e.material.roughness = matj.value("roughness", 0.40f);
                        e.material.emissiveColor = JsonToVec3(matj["emissiveColor"], glm::vec3(0.0f));
                        e.material.emissiveStrength = matj.value("emissiveStrength", 0.0f);
                        e.material.uvScale = matj.value("uvScale", 1.0f);
                        e.material.patternType = matj.value("patternType", 0);
                    }

                    if (ej.contains("light"))
                    {
                        const auto& lj = ej["light"];
                        e.light.enabled = lj.value("enabled", false);
                        e.light.isDirectional = lj.value("isDirectional", false);
                        e.light.color = JsonToVec3(lj["color"], glm::vec3(1.0f));
                        e.light.intensity = lj.value("intensity", 2.0f);
                        e.light.range = lj.value("range", 15.0f);
                    }

                    if (ej.contains("camera"))
                    {
                        const auto& cj = ej["camera"];
                        e.camera.enabled = cj.value("enabled", false);
                        e.camera.isPrimary = cj.value("isPrimary", false);
                        e.camera.fov = cj.value("fov", 55.0f);
                        e.camera.nearClip = cj.value("nearClip", 0.1f);
                        e.camera.farClip = cj.value("farClip", 350.0f);
                        e.camera.followTargetName = cj.value("followTargetName", "PlayerRover");
                        e.camera.followOffset = JsonToVec3(cj["followOffset"], glm::vec3(0.0f, 13.5f, -14.5f));
                        e.camera.followLookOffset = JsonToVec3(cj["followLookOffset"], glm::vec3(0.0f, 0.5f, 3.8f));
                        e.camera.smoothSpeed = cj.value("smoothSpeed", 8.0f);
                    }

                    if (ej.contains("rigidBody"))
                    {
                        const auto& rbj = ej["rigidBody"];
                        e.rigidBody.enabled = rbj.value("enabled", false);
                        e.rigidBody.isKinematic = rbj.value("isKinematic", false);
                        e.rigidBody.useGravity = rbj.value("useGravity", false);
                        e.rigidBody.freezeY = rbj.value("freezeY", true);
                        e.rigidBody.mass = rbj.value("mass", 1.0f);
                        e.rigidBody.drag = rbj.value("drag", 4.0f);
                        e.rigidBody.bounciness = rbj.value("bounciness", 0.2f);
                        e.rigidBody.velocity = JsonToVec3(rbj["velocity"], glm::vec3(0.0f));
                    }

                    if (ej.contains("collider"))
                    {
                        const auto& colj = ej["collider"];
                        e.collider.enabled = colj.value("enabled", false);
                        e.collider.isSphere = colj.value("isSphere", true);
                        e.collider.isTrigger = colj.value("isTrigger", false);
                        e.collider.boxHalfExtents = JsonToVec3(colj["boxHalfExtents"], glm::vec3(0.6f));
                        e.collider.sphereRadius = colj.value("sphereRadius", 0.75f);
                    }

                    if (ej.contains("audioSource"))
                    {
                        const auto& aj = ej["audioSource"];
                        e.audioSource.enabled = aj.value("enabled", false);
                        e.audioSource.soundName = aj.value("soundName", "laser");
                        e.audioSource.volume = aj.value("volume", 0.8f);
                        e.audioSource.pitch = aj.value("pitch", 1.0f);
                        e.audioSource.loop = aj.value("loop", false);
                        e.audioSource.spatial = aj.value("spatial", true);
                        e.audioSource.playOnAwake = aj.value("playOnAwake", false);
                    }

                    if (ej.contains("luaScript"))
                    {
                        const auto& sj = ej["luaScript"];
                        e.luaScript.enabled = sj.value("enabled", false);
                        e.luaScript.scriptPath = sj.value("scriptPath", "");
                        e.luaScript.initialized = false;
                    }

                    if (ej.contains("gameplay"))
                    {
                        const auto& gj = ej["gameplay"];
                        e.gameplay.enabled = gj.value("enabled", false);
                        e.gameplay.health = gj.value("health", 100.0f);
                        e.gameplay.maxHealth = gj.value("maxHealth", 100.0f);
                        e.gameplay.shield = gj.value("shield", 100.0f);
                        e.gameplay.maxShield = gj.value("maxShield", 100.0f);
                        e.gameplay.moveSpeed = gj.value("moveSpeed", 14.0f);
                        e.gameplay.fireRate = gj.value("fireRate", 0.18f);
                        e.gameplay.fireCooldown = gj.value("fireCooldown", 0.0f);
                        e.gameplay.damage = gj.value("damage", 25.0f);
                        e.gameplay.maxLifetime = gj.value("maxLifetime", 0.0f);
                        e.gameplay.scoreValue = gj.value("scoreValue", 100);
                        e.gameplay.subType = gj.value("subType", 0);
                        e.gameplay.state = gj.value("state", 0);
                        e.gameplay.extraKey = gj.value("extraKey", "");
                        if (gj.contains("targetPos")) e.gameplay.targetPos = JsonToVec3(gj["targetPos"]);
                        if (gj.contains("basePos"))   e.gameplay.basePos = JsonToVec3(gj["basePos"]);
                    }
                }
            }

            scene.enemiesRemainingInWave = static_cast<int>(scene.FindEntitiesByTag("Enemy").size());
            return true;
        }
        catch (const std::exception& ex)
        {
            Log::Error(std::string("Scene JSON deserialize error: ") + ex.what());
            return false;
        }
    }

    bool SceneSerializer::SaveToFile(const Scene& scene, const std::string& filePath)
    {
        try
        {
            std::filesystem::path p(filePath);
            if (p.has_parent_path())
                std::filesystem::create_directories(p.parent_path());

            std::ofstream out(filePath, std::ios::out | std::ios::trunc);
            if (!out)
            {
                Log::Error("Failed to open scene file for writing: " + filePath);
                return false;
            }
            out << SerializeToString(scene);
            Log::Info("Saved scene to: " + filePath);
            return true;
        }
        catch (const std::exception& ex)
        {
            Log::Error(std::string("SaveToFile failed: ") + ex.what());
            return false;
        }
    }

    bool SceneSerializer::LoadFromFile(Scene& scene, const std::string& filePath)
    {
        std::ifstream in(filePath);
        if (!in)
        {
            Log::Error("Failed to open scene file for reading: " + filePath);
            return false;
        }
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        bool ok = DeserializeFromString(scene, content);
        if (ok)
            Log::Info("Loaded scene from: " + filePath);
        return ok;
    }

    bool SceneSerializer::SaveProjectConfig(const GameProjectConfig& config, const std::string& filePath)
    {
        try
        {
            std::filesystem::path p(filePath);
            if (p.has_parent_path())
                std::filesystem::create_directories(p.parent_path());

            json j;
            j["gameTitle"] = config.gameTitle;
            j["startupScene"] = config.startupScene;
            j["windowWidth"] = config.windowWidth;
            j["windowHeight"] = config.windowHeight;
            j["preferredBackend"] = config.preferredBackend;
            j["maxWaves"] = config.maxWaves;

            std::ofstream out(filePath, std::ios::out | std::ios::trunc);
            if (!out) return false;
            out << j.dump(2);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool SceneSerializer::LoadProjectConfig(GameProjectConfig& config, const std::string& filePath)
    {
        std::ifstream in(filePath);
        if (!in) return false;
        try
        {
            json j = json::parse(in);
            config.gameTitle = j.value("gameTitle", config.gameTitle);
            config.startupScene = j.value("startupScene", config.startupScene);
            config.windowWidth = j.value("windowWidth", config.windowWidth);
            config.windowHeight = j.value("windowHeight", config.windowHeight);
            config.preferredBackend = j.value("preferredBackend", config.preferredBackend);
            config.maxWaves = j.value("maxWaves", config.maxWaves);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}
