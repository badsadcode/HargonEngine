#include "FPSGameManager.h"
#include "engine/core/Log.h"
#include "engine/core/InputMap.h"
#include "engine/animation/TweenSystem.h"
#include <nlohmann/json.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <ctime>

namespace Hargon
{
    using json = nlohmann::json;

    static constexpr float kMapScale = 1.0f / 32.0f; // 32 Quake units = 1.0 engine meter
    static constexpr float kPi = 3.14159265358979323846f;

    static inline uint64_t PackCell(int cx, int cy)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) | static_cast<uint32_t>(cy);
    }

    static inline glm::vec3 MapPosToEngine(float mx, float my, float mz)
    {
        // BadLevel Z-up (mx, my, mz) -> Hargon Y-up (mx * SCALE, mz * SCALE, my * SCALE)
        return glm::vec3(mx * kMapScale, mz * kMapScale, my * kMapScale);
    }

    static inline glm::vec3 MapNrmToEngine(float nx, float ny, float nz)
    {
        return glm::normalize(glm::vec3(nx, nz, ny));
    }

    const char* FPSGameManager::GetWeaponName(FPSWeaponType type)
    {
        switch (type)
        {
        case FPSWeaponType::RunicAxe:       return "1: Runic Battle-Axe";
        case FPSWeaponType::Shotgun:        return "2: Buckshot Shotgun";
        case FPSWeaponType::SuperNailgun:   return "3: Super Perforator Nailgun";
        case FPSWeaponType::RocketLauncher: return "4: Ogre Rocket Launcher";
        case FPSWeaponType::LightningGun:   return "5: Thunderbolt Shaft";
        default:                            return "Unknown Weapon";
        }
    }

    const char* FPSGameManager::GetEnemyName(FPSEnemyType type)
    {
        switch (type)
        {
        case FPSEnemyType::RottedGrunt:  return "Rotted Grunt";
        case FPSEnemyType::Enforcer:     return "Laser Enforcer";
        case FPSEnemyType::Rottweiler:   return "War Rottweiler";
        case FPSEnemyType::IronKnight:   return "Iron Death-Knight";
        case FPSEnemyType::HellKnight:   return "Hell Baron Knight";
        case FPSEnemyType::ChainsawOgre: return "Chainsaw Ogre";
        case FPSEnemyType::Fiend:        return "Horned Fiend";
        case FPSEnemyType::Scrag:        return "Acid Scrag Wizard";
        case FPSEnemyType::Vore:         return "Vore Shalrath";
        case FPSEnemyType::Shambler:     return "Elder Shambler";
        default:                         return "Monster";
        }
    }

    const char* FPSGameManager::GetCampaignTitle(int campaign)
    {
        return (campaign == 1)
            ? "CAMPAIGN I: THE ELDER WARD (Gothic / Runic / Catacombs)"
            : "CAMPAIGN II: THE IRON CITADEL (Bunker / Industrial / Slipgate Base)";
    }

    std::string FPSGameManager::GetLevelCode(int campaign, int levelIndex)
    {
        int c = std::clamp(campaign, 1, 2);
        int m = std::clamp(levelIndex, 1, 5);
        return "c" + std::to_string(c) + "m" + std::to_string(m);
    }

    void FPSGameManager::Init(Scene* scene, Renderer3D* renderer, AudioEngine* audio, Window* window, bool startInMenu)
    {
        m_Scene = scene;
        m_Renderer = renderer;
        m_Audio = audio;
        m_Window = window;

        LoadSettings("settings.json");
        ApplySettings();
        RefreshSaveSlots();

        LoadCampaignLevel(1, 1, true);
        SetMenuState(startInMenu ? FPSMenuState::MainMenu : FPSMenuState::None);
    }

    void FPSGameManager::SetMenuState(FPSMenuState state)
    {
        m_MenuState = state;
        if (m_Window)
        {
            if (m_MenuState == FPSMenuState::None)
                m_Window->SetCursorCaptured(true);
            else
                m_Window->SetCursorCaptured(false);
        }
    }

    void FPSGameManager::LoadSettings(const std::string& path)
    {
        if (!std::filesystem::exists(path))
            return;
        try
        {
            std::ifstream in(path);
            json j = json::parse(in);
            m_Settings.fovDegrees       = j.value("fovDegrees", 78.0f);
            m_Settings.brightness       = j.value("brightness", 1.05f);
            m_Settings.fogMultiplier    = j.value("fogMultiplier", 1.0f);
            m_Settings.dynamicLights    = j.value("dynamicLights", true);
            m_Settings.bloodGoreScale   = j.value("bloodGoreScale", 1.0f);
            m_Settings.fullscreen       = j.value("fullscreen", false);
            m_Settings.resolutionIndex  = j.value("resolutionIndex", 1);
            m_Settings.masterVolume     = j.value("masterVolume", 0.85f);
            m_Settings.sfxVolume        = j.value("sfxVolume", 0.90f);
            m_Settings.ambientVolume    = j.value("ambientVolume", 0.65f);
            m_Settings.muted            = j.value("muted", false);
            m_Settings.mouseSensitivity = j.value("mouseSensitivity", 0.18f);
        }
        catch (...) {}
    }

    void FPSGameManager::SaveSettings(const std::string& path)
    {
        try
        {
            json j;
            j["fovDegrees"]       = m_Settings.fovDegrees;
            j["brightness"]       = m_Settings.brightness;
            j["fogMultiplier"]    = m_Settings.fogMultiplier;
            j["dynamicLights"]    = m_Settings.dynamicLights;
            j["bloodGoreScale"]   = m_Settings.bloodGoreScale;
            j["fullscreen"]       = m_Settings.fullscreen;
            j["resolutionIndex"]  = m_Settings.resolutionIndex;
            j["masterVolume"]     = m_Settings.masterVolume;
            j["sfxVolume"]        = m_Settings.sfxVolume;
            j["ambientVolume"]    = m_Settings.ambientVolume;
            j["muted"]            = m_Settings.muted;
            j["mouseSensitivity"] = m_Settings.mouseSensitivity;

            std::ofstream out(path, std::ios::out | std::ios::trunc);
            out << j.dump(2);
        }
        catch (...) {}
    }

    void FPSGameManager::ApplySettings()
    {
        if (m_Audio)
        {
            m_Audio->SetMasterVolume(m_Settings.masterVolume);
            m_Audio->SetSFXVolume(m_Settings.sfxVolume);
            m_Audio->SetAmbientVolume(m_Settings.ambientVolume);
            m_Audio->SetMuted(m_Settings.muted);
        }
        if (m_Scene)
        {
            m_Scene->ambientIntensity = 0.48f * m_Settings.brightness;
        }
    }

    bool FPSGameManager::GenerateAndLoadLevel(int campaign, int levelIndex, int customSeed)
    {
        std::string code = GetLevelCode(campaign, levelIndex);
        std::string style = (campaign == 1) ? "gothic" : "industrial";
        std::string recipe = (campaign == 1) ? "fortress" : "industrial";

        char cmdBuf[1024];
        std::snprintf(cmdBuf, sizeof(cmdBuf),
            "python -c \"import sys, json; sys.path.insert(0, 'external/BadLevel'); "
            "from badlevel.recipes import get_recipe, compile_recipe, roomier, varied; "
            "from badlevel.generator import generate; "
            "from badlevel.export.ir_json import to_dict; "
            "from badlevel.export.mesh import build_mesh; "
            "cfg = compile_recipe(get_recipe('%s'), %d); cfg.arch_style='%s'; roomier(cfg, 0.45); varied(cfg, 0.45); "
            "lvl = generate(cfg); d = to_dict(lvl, True); d['level_code']='%s'; d['title']='Generated Map (Seed %d)'; "
            "d['campaign']=%d; d['level_index']=%d; d['style']='%s'; d['recipe']='%s'; "
            "mb = build_mesh(lvl); d['render_mesh']={m:g for m,g in mb.groups.items() if g['idx']}; "
            "open('assets/levels/%s.json','w').write(json.dumps(d))\"",
            recipe.c_str(), customSeed, style.c_str(), code.c_str(), customSeed, campaign, levelIndex, style.c_str(), recipe.c_str(), code.c_str());

        std::system(cmdBuf);
        return LoadCampaignLevel(campaign, levelIndex, false);
    }

    bool FPSGameManager::LoadCampaignLevel(int campaign, int levelIndex, bool resetPlayerInventory)
    {
        if (!m_Scene || !m_Renderer)
            return false;

        m_Campaign = std::clamp(campaign, 1, 2);
        m_LevelIndex = std::clamp(levelIndex, 1, 5);
        m_LevelCode = GetLevelCode(m_Campaign, m_LevelIndex);

        std::string jsonPath = "assets/levels/" + m_LevelCode + ".json";
        if (!std::filesystem::exists(jsonPath))
        {
            if (std::filesystem::exists("../" + jsonPath))
                jsonPath = "../" + jsonPath;
            else
            {
                Log::Error("BadLevel file not found: " + jsonPath);
                return false;
            }
        }

        std::ifstream in(jsonPath);
        if (!in)
            return false;

        json root = json::parse(in);

        m_LevelTitle  = root.value("title", m_LevelCode);
        m_LevelStyle  = root.value("style", (m_Campaign == 1) ? "gothic" : "industrial");
        m_LevelRecipe = root.value("recipe", "classic");
        m_LevelSeed   = root.value("seed", 101);

        m_Scene->Clear();
        m_Scene->sceneName = m_LevelCode + ": " + m_LevelTitle;

        // Configure Campaign & Style Atmosphere (Quake 1 Gothic vs Slipgate Base)
        if (m_Campaign == 1)
        {
            m_Scene->skyZenithColor     = glm::vec3(0.08f, 0.04f, 0.12f);
            m_Scene->skyHorizonColor    = glm::vec3(0.24f, 0.14f, 0.22f);
            m_Scene->groundAmbientColor = glm::vec3(0.14f, 0.11f, 0.09f);
            m_Scene->ambientIntensity   = 0.50f * m_Settings.brightness;
            m_Scene->fogDensity         = 0.014f * m_Settings.fogMultiplier;
        }
        else
        {
            m_Scene->skyZenithColor     = glm::vec3(0.04f, 0.09f, 0.14f);
            m_Scene->skyHorizonColor    = glm::vec3(0.16f, 0.25f, 0.30f);
            m_Scene->groundAmbientColor = glm::vec3(0.10f, 0.13f, 0.15f);
            m_Scene->ambientIntensity   = 0.52f * m_Settings.brightness;
            m_Scene->fogDensity         = 0.012f * m_Settings.fogMultiplier;
        }

        // Directional atmospheric key light
        {
            Entity& dirLight = m_Scene->CreateEntity("LevelSunLight", "Light");
            dirLight.mesh.enabled = false;
            dirLight.light.enabled = true;
            dirLight.light.isDirectional = true;
            dirLight.transform.rotation = glm::vec3(50.0f, -35.0f, 0.0f);
            dirLight.light.color = (m_Campaign == 1) ? glm::vec3(1.0f, 0.85f, 0.68f) : glm::vec3(0.85f, 0.94f, 1.0f);
            dirLight.light.intensity = 1.65f * m_Settings.brightness;
        }

        // 1. Parse BadLevel Column Spans & Diagonals for exact 3D collision & stair climbing
        m_ColumnSpans.clear();
        m_DiagonalCorners.clear();
        m_Rooms.clear();

        if (root.contains("grid"))
        {
            m_GridWidth  = root["grid"].value("width", 128);
            m_GridHeight = root["grid"].value("height", 128);
        }

        if (root.contains("spans") && root["spans"].is_array())
        {
            for (const auto& entry : root["spans"])
            {
                if (!entry.is_array() || entry.size() < 3) continue;
                int cx = entry[0].get<int>();
                int cy = entry[1].get<int>();
                std::vector<BadLevelSpan> spans;
                for (const auto& sp : entry[2])
                {
                    if (sp.is_array() && sp.size() >= 2)
                    {
                        spans.push_back({ sp[0].get<float>() * kMapScale, sp[1].get<float>() * kMapScale });
                    }
                }
                if (!spans.empty())
                    m_ColumnSpans[PackCell(cx, cy)] = std::move(spans);
            }
        }

        if (root.contains("diagonal") && root["diagonal"].is_array())
        {
            for (const auto& d : root["diagonal"])
            {
                if (d.is_array() && d.size() >= 3)
                {
                    m_DiagonalCorners[PackCell(d[0].get<int>(), d[1].get<int>())] = d[2].get<std::string>();
                }
            }
        }

        if (root.contains("rooms") && root["rooms"].is_array())
        {
            for (const auto& r : root["rooms"])
            {
                BadLevelRoomInfo ri;
                ri.id     = r.value("id", 0);
                ri.kind   = r.value("kind", "combat");
                ri.x      = r.value("x", 0);
                ri.y      = r.value("y", 0);
                ri.w      = r.value("w", 8);
                ri.h      = r.value("h", 8);
                ri.floorY = r.value("floor", 0.0f) * kMapScale;
                ri.ceilY  = r.value("ceil", 128.0f) * kMapScale;
                m_Rooms.push_back(ri);
            }
        }

        // 2. Upload BadLevel 3D Render Meshes (Walls, Floors, Ceilings, Sky) to Renderer3D
        if (root.contains("render_mesh") && root["render_mesh"].is_object())
        {
            for (auto& [matName, group] : root["render_mesh"].items())
            {
                if (matName == "sky")
                    continue; // Leave open courtyard roofs open to the 3D Skybox!

                const auto& posArr = group["pos"];
                const auto& nrmArr = group["nrm"];
                const auto& uvArr  = group["uv"];
                const auto& idxArr = group["idx"];

                std::vector<Vertex3D> verts;
                verts.reserve(posArr.size());

                for (size_t i = 0; i < posArr.size(); ++i)
                {
                    Vertex3D v{};
                    v.position = MapPosToEngine(posArr[i][0].get<float>(), posArr[i][1].get<float>(), posArr[i][2].get<float>());
                    v.normal   = MapNrmToEngine(nrmArr[i][0].get<float>(), nrmArr[i][1].get<float>(), nrmArr[i][2].get<float>());
                    v.uv       = glm::vec2(uvArr[i][0].get<float>(), uvArr[i][1].get<float>());
                    v.color    = glm::vec4(1.0f);
                    verts.push_back(v);
                }

                std::vector<uint32_t> indices;
                indices.reserve(idxArr.size());
                // Note: Swapping Y and Z in MapPosToEngine inverts handedness, so we flip triangle winding (i0, i2, i1)
                for (size_t i = 0; i + 2 < idxArr.size(); i += 3)
                {
                    indices.push_back(idxArr[i + 0].get<uint32_t>());
                    indices.push_back(idxArr[i + 2].get<uint32_t>());
                    indices.push_back(idxArr[i + 1].get<uint32_t>());
                }

                std::string meshKey = "badlevel_" + m_LevelCode + "_" + matName;
                m_Renderer->RegisterCustomMesh(meshKey, verts, indices);

                Entity& geoEnt = m_Scene->CreateEntity("LevelGeo_" + matName, "LevelGeometry");
                geoEnt.mesh.enabled = true;
                geoEnt.mesh.primitive = MeshPrimitive::CustomOBJ;
                geoEnt.mesh.customObjPath = meshKey;
                geoEnt.material.uvScale = 0.25f;

                // Assign Quake-1 inspired procedural texture patterns & palettes by Campaign & Style!
                if (matName == "wall")
                {
                    if (m_Campaign == 1)
                    {
                        if (m_LevelStyle == "egyptian")
                            geoEnt.material.albedoColor = glm::vec4(0.68f, 0.56f, 0.38f, 1.0f);
                        else if (m_LevelStyle == "castle")
                            geoEnt.material.albedoColor = glm::vec4(0.52f, 0.50f, 0.46f, 1.0f);
                        else
                            geoEnt.material.albedoColor = glm::vec4(0.55f, 0.48f, 0.40f, 1.0f);
                        geoEnt.material.patternType = 5; // Gothic Weathered Bricks
                        geoEnt.material.roughness = 0.78f;
                        geoEnt.material.metallic = 0.10f;
                    }
                    else
                    {
                        if (m_LevelStyle == "scifi")
                            geoEnt.material.albedoColor = glm::vec4(0.42f, 0.52f, 0.58f, 1.0f);
                        else if (m_LevelStyle == "bunker")
                            geoEnt.material.albedoColor = glm::vec4(0.50f, 0.49f, 0.45f, 1.0f);
                        else
                            geoEnt.material.albedoColor = glm::vec4(0.52f, 0.44f, 0.36f, 1.0f);
                        geoEnt.material.patternType = 8; // Riveted Rusted Iron & Base Panels
                        geoEnt.material.roughness = 0.52f;
                        geoEnt.material.metallic = 0.45f;
                    }
                }
                else if (matName == "floor")
                {
                    if (m_Campaign == 1)
                    {
                        geoEnt.material.albedoColor = glm::vec4(0.46f, 0.40f, 0.34f, 1.0f);
                        geoEnt.material.patternType = 6; // Runic Flagstone Floor Tiles
                        geoEnt.material.roughness = 0.72f;
                        geoEnt.material.metallic = 0.12f;
                    }
                    else
                    {
                        geoEnt.material.albedoColor = glm::vec4(0.38f, 0.42f, 0.45f, 1.0f);
                        geoEnt.material.patternType = 9; // Industrial Metal Grating & Diamond Plate
                        geoEnt.material.roughness = 0.45f;
                        geoEnt.material.metallic = 0.55f;
                    }
                }
                else // ceiling
                {
                    geoEnt.material.albedoColor = (m_Campaign == 1)
                        ? glm::vec4(0.38f, 0.32f, 0.28f, 1.0f)
                        : glm::vec4(0.30f, 0.35f, 0.40f, 1.0f);
                    geoEnt.material.patternType = 7; // Vaulted Coffered Ceiling
                    geoEnt.material.roughness = 0.75f;
                    geoEnt.material.metallic = 0.20f;
                }
            }
        }

        // Reset level counters
        m_HasRedKey = false;
        m_HasBlueKey = false;
        m_KillsInLevel = 0;
        m_TotalMonstersInLevel = 0;
        m_SecretsFound = 0;
        m_TotalSecrets = 0;
        m_LevelTime = 0.0f;
        m_BloodDecalCount = 0;

        if (resetPlayerInventory)
        {
            m_PlayerHealth = 100.0f;
            m_PlayerArmor = 50.0f;
            m_CurrentWeapon = FPSWeaponType::Shotgun;
            m_WeaponUnlocked = { true, true, true, false, false };
            m_Ammo = { 999, 36, 80, 12, 40 };
        }
        else
        {
            m_PlayerHealth = std::max(m_PlayerHealth, 75.0f);
        }

        // 3. Parse BadLevel Gameplay Entities (Player Start, Doors, Lifts, Keys, Enemies, Weapons, Pickups, Lights)
        int enemySpawnCounter = 0;
        int weaponPickupCounter = 0;
        int ammoPickupCounter = 0;
        int lightCounter = 0;

        if (root.contains("entities") && root["entities"].is_array())
        {
            for (const auto& ent : root["entities"])
            {
                std::string kind = ent.value("kind", "");
                const auto& posArr = ent["pos"];
                glm::vec3 pos = MapPosToEngine(posArr[0].get<float>(), posArr[1].get<float>(), posArr[2].get<float>());
                const auto& props = ent.contains("props") ? ent["props"] : json::object();

                if (kind == "player_start")
                {
                    float qAngle = props.value("angle", 0.0f);
                    // Quake angle: 0 = +X, 90 = +Y (which is +Z in Engine). Engine yaw: 0 = +Z, 90 = +X
                    m_PlayerYaw = 90.0f - qAngle;
                    m_PlayerPitch = 0.0f;
                    float fl = pos.y - 1.0f, cl = pos.y + 4.0f;
                    if (QueryColumnFloorCeil(pos.x, pos.z, pos.y, fl, cl))
                        m_PlayerFeetPos = glm::vec3(pos.x, fl, pos.z);
                    else
                        m_PlayerFeetPos = glm::vec3(pos.x, pos.y - 0.8f, pos.z);
                    m_PlayerVelocity = glm::vec3(0.0f);
                }
                else if (kind == "enemy")
                {
                    int tier = props.value("tier", 1);
                    // Deterministic variety across all 10 enemy types based on campaign, level, and tier!
                    FPSEnemyType eType = FPSEnemyType::RottedGrunt;
                    int roll = (enemySpawnCounter + m_LevelIndex * 2 + (m_Campaign - 1) * 3) % 10;
                    if (tier == 1)
                    {
                        const FPSEnemyType tier1Pool[4] = {
                            FPSEnemyType::RottedGrunt,
                            FPSEnemyType::Rottweiler,
                            FPSEnemyType::Enforcer,
                            FPSEnemyType::IronKnight
                        };
                        eType = tier1Pool[enemySpawnCounter % 4];
                    }
                    else if (tier == 2)
                    {
                        const FPSEnemyType tier2Pool[4] = {
                            FPSEnemyType::ChainsawOgre,
                            FPSEnemyType::HellKnight,
                            FPSEnemyType::Fiend,
                            FPSEnemyType::Scrag
                        };
                        eType = tier2Pool[enemySpawnCounter % 4];
                    }
                    else
                    {
                        const FPSEnemyType tier3Pool[4] = {
                            FPSEnemyType::Vore,
                            FPSEnemyType::Shambler,
                            FPSEnemyType::Fiend,
                            FPSEnemyType::HellKnight
                        };
                        eType = tier3Pool[enemySpawnCounter % 4];
                    }
                    // Ensure all 10 enemy types appear across the campaign
                    if (enemySpawnCounter < 10 && m_LevelIndex >= 3)
                        eType = static_cast<FPSEnemyType>(roll);

                    float fl = pos.y - 0.75f, cl = pos.y + 4.0f;
                    QueryColumnFloorCeil(pos.x, pos.z, pos.y, fl, cl);
                    SpawnEnemyByType(eType, glm::vec3(pos.x, fl, pos.z), static_cast<float>((enemySpawnCounter * 73) % 360));
                    ++enemySpawnCounter;
                }
                else if (kind == "door" || kind == "secret_door")
                {
                    if (!ent.contains("bounds")) continue;
                    glm::vec3 lo = MapPosToEngine(ent["bounds"][0][0], ent["bounds"][0][1], ent["bounds"][0][2]);
                    glm::vec3 hi = MapPosToEngine(ent["bounds"][1][0], ent["bounds"][1][1], ent["bounds"][1][2]);
                    glm::vec3 center = (lo + hi) * 0.5f;
                    glm::vec3 size = glm::abs(hi - lo);

                    bool isSecret = (kind == "secret_door");
                    if (isSecret) ++m_TotalSecrets;

                    std::string keyColor = props.value("key", "");
                    Entity& door = m_Scene->CreateEntity(isSecret ? "SecretDoor" : ("LockedDoor_" + keyColor), "Door");
                    door.transform.position = center;
                    door.transform.scale = size;
                    door.mesh.enabled = true;
                    door.mesh.primitive = MeshPrimitive::Cube;
                    door.material.patternType = isSecret ? ((m_Campaign == 1) ? 5 : 8) : 10;
                    if (keyColor == "red")
                    {
                        door.material.albedoColor = glm::vec4(0.78f, 0.18f, 0.15f, 1.0f);
                        door.material.emissiveColor = glm::vec3(1.0f, 0.20f, 0.15f);
                        door.material.emissiveStrength = 1.4f;
                    }
                    else if (keyColor == "blue")
                    {
                        door.material.albedoColor = glm::vec4(0.18f, 0.38f, 0.85f, 1.0f);
                        door.material.emissiveColor = glm::vec3(0.20f, 0.50f, 1.0f);
                        door.material.emissiveStrength = 1.4f;
                    }
                    else
                    {
                        door.material.albedoColor = glm::vec4(0.48f, 0.42f, 0.36f, 1.0f);
                    }
                    door.collider.enabled = true;
                    door.collider.isSphere = false;
                    door.collider.boxHalfExtents = size * 0.5f;

                    door.gameplay.enabled = true;
                    door.gameplay.subType = isSecret ? 1 : 0;
                    door.gameplay.state = 0; // 0 = closed, 1 = opening, 2 = open
                    door.gameplay.extraKey = keyColor;
                    door.gameplay.basePos = center;
                    door.gameplay.targetPos = center + glm::vec3(0.0f, size.y - 0.15f, 0.0f);
                }
                else if (kind == "lift")
                {
                    if (!ent.contains("bounds")) continue;
                    glm::vec3 lo = MapPosToEngine(ent["bounds"][0][0], ent["bounds"][0][1], ent["bounds"][0][2]);
                    glm::vec3 hi = MapPosToEngine(ent["bounds"][1][0], ent["bounds"][1][1], ent["bounds"][1][2]);
                    glm::vec3 size = glm::abs(hi - lo);
                    float lowY  = props.value("low", 0.0f) * kMapScale;
                    float highY = props.value("high", 128.0f) * kMapScale;

                    Entity& lift = m_Scene->CreateEntity("ElevatorLift", "Lift");
                    lift.transform.position = glm::vec3((lo.x + hi.x) * 0.5f, lowY - size.y * 0.5f, (lo.z + hi.z) * 0.5f);
                    lift.transform.scale = glm::max(size, glm::vec3(1.5f, 0.4f, 1.5f));
                    lift.mesh.enabled = true;
                    lift.mesh.primitive = MeshPrimitive::Cube;
                    lift.material.albedoColor = glm::vec4(0.85f, 0.65f, 0.20f, 1.0f);
                    lift.material.patternType = 9;
                    lift.gameplay.enabled = true;
                    lift.gameplay.basePos = glm::vec3(lift.transform.position.x, lowY, lift.transform.position.z);
                    lift.gameplay.targetPos = glm::vec3(lift.transform.position.x, highY, lift.transform.position.z);
                    lift.gameplay.state = 0;
                }
                else if (kind == "key")
                {
                    std::string color = props.value("color", "red");
                    SpawnPickupEntity(color == "blue" ? FPSPickupType::KeyBlue : FPSPickupType::KeyRed, pos);
                }
                else if (kind == "exit")
                {
                    Entity& slipgate = m_Scene->CreateEntity("ExitSlipgate", "Exit");
                    slipgate.transform.position = pos;
                    slipgate.transform.scale = glm::vec3(1.8f, 2.2f, 1.8f);
                    slipgate.mesh.enabled = true;
                    slipgate.mesh.primitive = MeshPrimitive::PropSlipgate;
                    slipgate.material.albedoColor = glm::vec4(0.85f, 0.25f, 1.0f, 1.0f);
                    slipgate.material.emissiveColor = glm::vec3(0.75f, 0.20f, 1.0f);
                    slipgate.material.emissiveStrength = 3.2f;
                    slipgate.material.patternType = 11; // Swirling Slipgate portal
                    slipgate.light.enabled = true;
                    slipgate.light.color = glm::vec3(0.85f, 0.25f, 1.0f);
                    slipgate.light.intensity = 4.0f;
                    slipgate.light.range = 12.0f;
                }
                else if (kind == "weapon")
                {
                    // Cycle through all 4 ranged weapons so the player can find and wield all 5 weapons!
                    FPSPickupType wTypes[4] = {
                        FPSPickupType::WeaponShotgun,
                        FPSPickupType::WeaponSuperNailgun,
                        FPSPickupType::WeaponRocketLauncher,
                        FPSPickupType::WeaponLightningGun
                    };
                    SpawnPickupEntity(wTypes[weaponPickupCounter % 4], pos + glm::vec3(0.0f, 0.45f, 0.0f));
                    ++weaponPickupCounter;
                }
                else if (kind == "health")
                {
                    SpawnPickupEntity(FPSPickupType::Health, pos + glm::vec3(0.0f, 0.35f, 0.0f));
                }
                else if (kind == "megahealth")
                {
                    SpawnPickupEntity(FPSPickupType::MegaHealth, pos + glm::vec3(0.0f, 0.45f, 0.0f));
                }
                else if (kind == "armor")
                {
                    SpawnPickupEntity(FPSPickupType::Armor, pos + glm::vec3(0.0f, 0.45f, 0.0f));
                }
                else if (kind == "ammo")
                {
                    FPSPickupType aTypes[4] = {
                        FPSPickupType::AmmoShells,
                        FPSPickupType::AmmoNails,
                        FPSPickupType::AmmoRockets,
                        FPSPickupType::AmmoCells
                    };
                    SpawnPickupEntity(aTypes[ammoPickupCounter % 4], pos + glm::vec3(0.0f, 0.35f, 0.0f));
                    ++ammoPickupCounter;
                }
                else if (kind == "light")
                {
                    // Spawn atmospheric point lights + ceiling/high-wall Torch / Tech Lamp fixtures
                    if ((lightCounter % 2) == 0)
                    {
                        Entity& torch = m_Scene->CreateEntity("AtmosphericLight_" + std::to_string(lightCounter), "Light");
                        torch.transform.position = pos;
                        torch.transform.scale = glm::vec3(0.35f);
                        torch.mesh.enabled = false;
                        torch.mesh.primitive = MeshPrimitive::PropTorch;
                        torch.material.albedoColor = glm::vec4(1.0f, 0.72f, 0.25f, 1.0f);
                        torch.material.emissiveColor = (m_Campaign == 1) ? glm::vec3(1.0f, 0.55f, 0.12f) : glm::vec3(0.35f, 0.85f, 1.0f);
                        torch.material.emissiveStrength = 2.8f;

                        torch.light.enabled = m_Settings.dynamicLights;
                        torch.light.isDirectional = false;
                        torch.light.color = (m_Campaign == 1) ? glm::vec3(1.0f, 0.62f, 0.22f) : glm::vec3(0.55f, 0.85f, 1.0f);
                        torch.light.intensity = 2.8f * m_Settings.brightness;
                        torch.light.range = 14.5f;
                    }
                    ++lightCounter;
                }
            }
        }

        // Spawn all 5 weapons in an armory rack behind/beside the player's starting spot so the corridor view ahead is clear
        {
            glm::vec3 fwd(std::sin(glm::radians(m_PlayerYaw)), 0.0f, std::cos(glm::radians(m_PlayerYaw)));
            glm::vec3 right(fwd.z, 0.0f, -fwd.x);
            SpawnPickupEntity(FPSPickupType::WeaponSuperNailgun,   m_PlayerFeetPos - fwd * 1.6f - right * 1.2f + glm::vec3(0.0f, 0.45f, 0.0f));
            SpawnPickupEntity(FPSPickupType::WeaponRocketLauncher, m_PlayerFeetPos - fwd * 1.6f + right * 0.0f + glm::vec3(0.0f, 0.45f, 0.0f));
            SpawnPickupEntity(FPSPickupType::WeaponLightningGun,   m_PlayerFeetPos - fwd * 1.6f + right * 1.2f + glm::vec3(0.0f, 0.45f, 0.0f));
        }

        // 4. Populate Atmospheric Decorations across rooms (Braziers, Pillars, Hanging Cages, Obelisks, Altars, Explosive Barrels, Crates)
        SpawnAtmosphericDecorations();

        // 5. Ensure First-Person Weapon ViewModel & Player Light exist
        EnsureViewModelEntity();

        m_StatusMessage = "ENTERED " + m_LevelCode + ": " + m_LevelTitle;
        m_StatusMessageTimer = 4.0f;

        Log::Gameplay("Loaded BadLevel map " + m_LevelCode + " (" + m_LevelTitle + ") with " +
            std::to_string(m_TotalMonstersInLevel) + " monsters and " + std::to_string(m_Rooms.size()) + " rooms.");
        return true;
    }

    void FPSGameManager::SpawnAtmosphericDecorations()
    {
        if (!m_Scene) return;

        int propCounter = 0;
        for (const auto& rm : m_Rooms)
        {
            if (rm.w < 6 || rm.h < 6)
                continue;

            float cx = (rm.x + rm.w * 0.5f) * 32.0f * kMapScale;
            float cz = (rm.y + rm.h * 0.5f) * 32.0f * kMapScale;
            float fl = rm.floorY;
            float cl = rm.ceilY;
            QueryColumnFloorCeil(cx, cz, rm.floorY + 0.5f, fl, cl);

            // Corner Braziers or Explosive Barrels
            float offsets[4][2] = {
                { (rm.x + 1.8f) * 32.0f * kMapScale, (rm.y + 1.8f) * 32.0f * kMapScale },
                { (rm.x + rm.w - 1.8f) * 32.0f * kMapScale, (rm.y + 1.8f) * 32.0f * kMapScale },
                { (rm.x + 1.8f) * 32.0f * kMapScale, (rm.y + rm.h - 1.8f) * 32.0f * kMapScale },
                { (rm.x + rm.w - 1.8f) * 32.0f * kMapScale, (rm.y + rm.h - 1.8f) * 32.0f * kMapScale }
            };

            for (int k = 0; k < 4; ++k)
            {
                float px = offsets[k][0];
                float pz = offsets[k][1];
                float cFl = fl, cCl = cl;
                if (!QueryColumnFloorCeil(px, pz, rm.floorY + 0.5f, cFl, cCl))
                    continue;
                if (cCl - cFl < 2.2f)
                    continue;

                if ((k + rm.id) % 2 == 0)
                {
                    // Flickering Fire Brazier
                    Entity& brazier = m_Scene->CreateEntity("Brazier_" + std::to_string(++propCounter), "Decoration");
                    brazier.transform.position = glm::vec3(px, cFl + 0.45f, pz);
                    brazier.transform.scale = glm::vec3(0.85f);
                    brazier.mesh.enabled = true;
                    brazier.mesh.primitive = MeshPrimitive::PropBrazier;
                    brazier.material.albedoColor = glm::vec4(0.95f, 0.55f, 0.18f, 1.0f);
                    brazier.material.emissiveColor = glm::vec3(1.0f, 0.50f, 0.12f);
                    brazier.material.emissiveStrength = 2.5f;
                }
                else
                {
                    // Shootable Explosive Barrel!
                    Entity& barrel = m_Scene->CreateEntity("ExplosiveBarrel_" + std::to_string(++propCounter), "Barrel");
                    barrel.transform.position = glm::vec3(px, cFl + 0.48f, pz);
                    barrel.transform.scale = glm::vec3(0.90f);
                    barrel.mesh.enabled = true;
                    barrel.mesh.primitive = MeshPrimitive::PropBarrel;
                    barrel.material.albedoColor = glm::vec4(0.38f, 0.52f, 0.25f, 1.0f);
                    barrel.material.patternType = 8;
                    barrel.gameplay.enabled = true;
                    barrel.gameplay.health = 30.0f;
                    barrel.gameplay.maxHealth = 30.0f;
                }
            }

            // Hanging Cages from high ceilings & Sacrificial Altars / Obelisks in Combat/Arena/Hub rooms
            if (rm.kind == "arena" || rm.kind == "hub" || rm.kind == "combat")
            {
                if (cl - fl >= 4.2f)
                {
                    Entity& cage = m_Scene->CreateEntity("HangingCage_" + std::to_string(++propCounter), "Decoration");
                    cage.transform.position = glm::vec3(cx + 1.5f, cl - 1.1f, cz - 1.5f);
                    cage.transform.scale = glm::vec3(1.1f);
                    cage.mesh.enabled = true;
                    cage.mesh.primitive = MeshPrimitive::PropCage;
                    cage.material.albedoColor = glm::vec4(0.32f, 0.30f, 0.28f, 1.0f);
                    cage.material.metallic = 0.75f;
                    cage.material.roughness = 0.45f;
                }

                if (rm.kind == "arena" && (rm.id % 2 == 0))
                {
                    Entity& altar = m_Scene->CreateEntity("RunicAltar_" + std::to_string(++propCounter), "Decoration");
                    altar.transform.position = glm::vec3(cx - 1.8f, fl + 0.45f, cz + 1.8f);
                    altar.transform.scale = glm::vec3(1.25f);
                    altar.mesh.enabled = true;
                    altar.mesh.primitive = (m_Campaign == 1) ? MeshPrimitive::PropAltar : MeshPrimitive::PropObelisk;
                    altar.material.albedoColor = glm::vec4(0.58f, 0.25f, 0.22f, 1.0f);
                    altar.material.patternType = 5;
                }
            }
        }
    }

    void FPSGameManager::SpawnEnemyByType(FPSEnemyType type, const glm::vec3& feetPos, float yaw)
    {
        if (!m_Scene) return;

        ++m_TotalMonstersInLevel;
        int idx = static_cast<int>(type);

        struct EnemySpec
        {
            MeshPrimitive prim;
            float hp;
            float speed;
            float damage;
            float fireRate;
            glm::vec3 scale;
            glm::vec4 color;
            float heightOffset;
            int score;
        };

        static const EnemySpec kSpecs[10] = {
            // 0: RottedGrunt (Shotgun hitscan)
            { MeshPrimitive::EnemyGrunt,      45.0f,  4.2f, 10.0f, 1.4f, {1.10f, 1.15f, 1.10f}, {0.58f, 0.48f, 0.36f, 1.0f}, 0.85f, 100 },
            // 1: Enforcer (Laser blaster)
            { MeshPrimitive::EnemyEnforcer,   70.0f,  4.5f, 14.0f, 1.1f, {1.18f, 1.22f, 1.18f}, {0.45f, 0.55f, 0.42f, 1.0f}, 0.90f, 150 },
            // 2: Rottweiler (Fast melee charger)
            { MeshPrimitive::EnemyRottweiler, 35.0f,  7.6f, 12.0f, 0.8f, {1.05f, 0.95f, 1.15f}, {0.55f, 0.32f, 0.20f, 1.0f}, 0.65f, 100 },
            // 3: IronKnight (Armored melee slasher)
            { MeshPrimitive::EnemyKnight,     75.0f,  5.4f, 16.0f, 0.9f, {1.15f, 1.20f, 1.15f}, {0.68f, 0.72f, 0.78f, 1.0f}, 0.90f, 150 },
            // 4: HellKnight (Magic spread + sword)
            { MeshPrimitive::EnemyHellKnight, 135.0f, 5.0f, 20.0f, 1.5f, {1.30f, 1.38f, 1.30f}, {0.68f, 0.25f, 0.18f, 1.0f}, 1.00f, 250 },
            // 5: ChainsawOgre (Grenade launcher + chainsaw)
            { MeshPrimitive::EnemyOgre,       120.0f, 4.4f, 22.0f, 1.7f, {1.32f, 1.28f, 1.32f}, {0.65f, 0.52f, 0.36f, 1.0f}, 0.95f, 220 },
            // 6: Fiend (Aggressive leaping demon)
            { MeshPrimitive::EnemyFiend,      150.0f, 7.2f, 25.0f, 1.0f, {1.35f, 1.25f, 1.35f}, {0.75f, 0.28f, 0.22f, 1.0f}, 0.90f, 280 },
            // 7: Scrag (Hovering acid wizard)
            { MeshPrimitive::EnemyScrag,      80.0f,  5.2f, 15.0f, 1.3f, {1.20f, 1.20f, 1.20f}, {0.65f, 0.85f, 0.45f, 1.0f}, 1.45f, 180 },
            // 8: Vore (Homing violet spike pods)
            { MeshPrimitive::EnemyVore,       190.0f, 4.6f, 26.0f, 2.0f, {1.38f, 1.35f, 1.38f}, {0.68f, 0.40f, 0.78f, 1.0f}, 0.95f, 350 },
            // 9: Shambler (Lightning horror boss)
            { MeshPrimitive::EnemyShambler,   280.0f, 4.8f, 32.0f, 1.8f, {1.65f, 1.68f, 1.65f}, {0.90f, 0.85f, 0.76f, 1.0f}, 1.20f, 500 }
        };

        const EnemySpec& sp = kSpecs[idx];
        Entity& e = m_Scene->CreateEntity(std::string(GetEnemyName(type)) + "_" + std::to_string(m_TotalMonstersInLevel), "Enemy");
        e.transform.position = feetPos + glm::vec3(0.0f, sp.heightOffset, 0.0f);
        e.transform.rotation = glm::vec3(0.0f, yaw, 0.0f);
        e.transform.scale    = sp.scale;

        e.mesh.enabled = true;
        e.mesh.primitive = sp.prim;

        e.material.albedoColor = sp.color;
        e.material.roughness = 0.55f;
        e.material.metallic = (type == FPSEnemyType::IronKnight || type == FPSEnemyType::Enforcer) ? 0.65f : 0.15f;
        e.material.patternType = 14; // Monster flesh/scale detail

        e.collider.enabled = true;
        e.collider.isSphere = true;
        e.collider.sphereRadius = 0.65f * sp.scale.x;

        e.gameplay.enabled = true;
        e.gameplay.subType = idx;
        e.gameplay.maxHealth = sp.hp;
        e.gameplay.health = sp.hp;
        e.gameplay.moveSpeed = sp.speed;
        e.gameplay.damage = sp.damage;
        e.gameplay.fireRate = sp.fireRate;
        e.gameplay.fireCooldown = 0.5f + (m_TotalMonstersInLevel % 5) * 0.25f;
        e.gameplay.scoreValue = sp.score;
        e.gameplay.basePos = e.transform.position;
        e.gameplay.state = 0; // 0=Idle/Patrol, 1=Alert/Chase
    }

    void FPSGameManager::SpawnPickupEntity(FPSPickupType type, const glm::vec3& pos)
    {
        if (!m_Scene) return;

        Entity& p = m_Scene->CreateEntity("Pickup_" + std::to_string(static_cast<int>(type)), "Pickup");
        p.transform.position = pos;
        p.gameplay.enabled = true;
        p.gameplay.subType = static_cast<int>(type);
        p.gameplay.basePos = pos;
        p.mesh.enabled = true;

        switch (type)
        {
        case FPSPickupType::Health:
            p.mesh.primitive = MeshPrimitive::PropCrate;
            p.transform.scale = glm::vec3(0.55f);
            p.material.albedoColor = glm::vec4(0.90f, 0.92f, 0.95f, 1.0f);
            p.material.emissiveColor = glm::vec3(0.95f, 0.20f, 0.20f);
            p.material.emissiveStrength = 1.2f;
            break;
        case FPSPickupType::MegaHealth:
            p.mesh.primitive = MeshPrimitive::Crystal;
            p.transform.scale = glm::vec3(0.85f);
            p.material.albedoColor = glm::vec4(1.0f, 0.25f, 0.35f, 1.0f);
            p.material.emissiveColor = glm::vec3(1.0f, 0.20f, 0.30f);
            p.material.emissiveStrength = 2.8f;
            break;
        case FPSPickupType::Armor:
            p.mesh.primitive = MeshPrimitive::Torus;
            p.transform.scale = glm::vec3(0.75f);
            p.material.albedoColor = glm::vec4(0.95f, 0.80f, 0.20f, 1.0f);
            p.material.emissiveColor = glm::vec3(0.85f, 0.65f, 0.15f);
            p.material.emissiveStrength = 1.6f;
            break;
        case FPSPickupType::AmmoShells:
        case FPSPickupType::AmmoNails:
        case FPSPickupType::AmmoRockets:
        case FPSPickupType::AmmoCells:
            p.mesh.primitive = MeshPrimitive::PropCrate;
            p.transform.scale = glm::vec3(0.50f);
            p.material.albedoColor = glm::vec4(0.55f, 0.65f, 0.40f, 1.0f);
            p.material.patternType = 13;
            break;
        case FPSPickupType::WeaponShotgun:
            p.mesh.primitive = MeshPrimitive::WeaponShotgun;
            p.transform.scale = glm::vec3(1.25f);
            p.material.albedoColor = glm::vec4(0.85f, 0.65f, 0.35f, 1.0f);
            p.material.emissiveColor = glm::vec3(0.6f, 0.4f, 0.1f);
            p.material.emissiveStrength = 1.2f;
            break;
        case FPSPickupType::WeaponSuperNailgun:
            p.mesh.primitive = MeshPrimitive::WeaponSuperNailgun;
            p.transform.scale = glm::vec3(1.25f);
            p.material.albedoColor = glm::vec4(0.70f, 0.78f, 0.88f, 1.0f);
            p.material.emissiveColor = glm::vec3(0.3f, 0.6f, 0.9f);
            p.material.emissiveStrength = 1.4f;
            break;
        case FPSPickupType::WeaponRocketLauncher:
            p.mesh.primitive = MeshPrimitive::WeaponRocketLauncher;
            p.transform.scale = glm::vec3(1.25f);
            p.material.albedoColor = glm::vec4(0.95f, 0.55f, 0.22f, 1.0f);
            p.material.emissiveColor = glm::vec3(0.9f, 0.4f, 0.1f);
            p.material.emissiveStrength = 1.6f;
            break;
        case FPSPickupType::WeaponLightningGun:
            p.mesh.primitive = MeshPrimitive::WeaponLightningGun;
            p.transform.scale = glm::vec3(1.25f);
            p.material.albedoColor = glm::vec4(0.35f, 0.90f, 1.0f, 1.0f);
            p.material.emissiveColor = glm::vec3(0.25f, 0.85f, 1.0f);
            p.material.emissiveStrength = 2.2f;
            break;
        case FPSPickupType::KeyRed:
            p.mesh.primitive = MeshPrimitive::Crystal;
            p.transform.scale = glm::vec3(0.90f);
            p.material.albedoColor = glm::vec4(1.0f, 0.18f, 0.15f, 1.0f);
            p.material.emissiveColor = glm::vec3(1.0f, 0.15f, 0.12f);
            p.material.emissiveStrength = 3.5f;
            p.light.enabled = true;
            p.light.color = glm::vec3(1.0f, 0.2f, 0.15f);
            p.light.intensity = 3.0f;
            p.light.range = 8.0f;
            break;
        case FPSPickupType::KeyBlue:
            p.mesh.primitive = MeshPrimitive::Crystal;
            p.transform.scale = glm::vec3(0.90f);
            p.material.albedoColor = glm::vec4(0.20f, 0.50f, 1.0f, 1.0f);
            p.material.emissiveColor = glm::vec3(0.20f, 0.55f, 1.0f);
            p.material.emissiveStrength = 3.5f;
            p.light.enabled = true;
            p.light.color = glm::vec3(0.2f, 0.55f, 1.0f);
            p.light.intensity = 3.0f;
            p.light.range = 8.0f;
            break;
        }
    }

    void FPSGameManager::EnsureViewModelEntity()
    {
        if (!m_Scene) return;

        Entity* vm = m_Scene->GetEntity(m_ViewModelEntity);
        if (!vm)
        {
            Entity& e = m_Scene->CreateEntity("FirstPersonWeaponViewModel", "ViewModel");
            m_ViewModelEntity = e.id;
            vm = &e;
        }
        vm->mesh.enabled = true;
        SelectWeapon(static_cast<int>(m_CurrentWeapon));

        Entity* pl = m_Scene->GetEntity(m_PlayerLightEntity);
        if (!pl)
        {
            Entity& l = m_Scene->CreateEntity("PlayerTorchLight", "MuzzleFlash");
            l.mesh.enabled = false;
            l.light.enabled = true;
            l.light.isDirectional = false;
            l.light.color = glm::vec3(1.0f, 0.85f, 0.65f);
            l.light.intensity = 1.6f;
            l.light.range = 14.0f;
            m_PlayerLightEntity = l.id;
        }
    }

    void FPSGameManager::SelectWeapon(int weaponIndex)
    {
        if (weaponIndex < 0 || weaponIndex >= 5)
            return;
        if (!m_WeaponUnlocked[weaponIndex])
            return;

        m_CurrentWeapon = static_cast<FPSWeaponType>(weaponIndex);
        if (Entity* vm = m_Scene ? m_Scene->GetEntity(m_ViewModelEntity) : nullptr)
        {
            switch (m_CurrentWeapon)
            {
            case FPSWeaponType::RunicAxe:
                vm->mesh.primitive = MeshPrimitive::WeaponAxe;
                vm->material.albedoColor = glm::vec4(0.82f, 0.84f, 0.88f, 1.0f);
                vm->material.metallic = 0.75f;
                vm->material.roughness = 0.28f;
                vm->material.emissiveStrength = 0.0f;
                break;
            case FPSWeaponType::Shotgun:
                vm->mesh.primitive = MeshPrimitive::WeaponShotgun;
                vm->material.albedoColor = glm::vec4(0.75f, 0.65f, 0.55f, 1.0f);
                vm->material.metallic = 0.60f;
                vm->material.roughness = 0.35f;
                vm->material.emissiveStrength = 0.0f;
                break;
            case FPSWeaponType::SuperNailgun:
                vm->mesh.primitive = MeshPrimitive::WeaponSuperNailgun;
                vm->material.albedoColor = glm::vec4(0.70f, 0.74f, 0.80f, 1.0f);
                vm->material.metallic = 0.75f;
                vm->material.roughness = 0.30f;
                vm->material.emissiveStrength = 0.0f;
                break;
            case FPSWeaponType::RocketLauncher:
                vm->mesh.primitive = MeshPrimitive::WeaponRocketLauncher;
                vm->material.albedoColor = glm::vec4(0.72f, 0.58f, 0.42f, 1.0f);
                vm->material.metallic = 0.68f;
                vm->material.roughness = 0.35f;
                vm->material.emissiveStrength = 0.0f;
                break;
            case FPSWeaponType::LightningGun:
                vm->mesh.primitive = MeshPrimitive::WeaponLightningGun;
                vm->material.albedoColor = glm::vec4(0.55f, 0.88f, 1.0f, 1.0f);
                vm->material.metallic = 0.80f;
                vm->material.roughness = 0.20f;
                vm->material.emissiveColor = glm::vec3(0.25f, 0.85f, 1.0f);
                vm->material.emissiveStrength = 1.2f;
                break;
            default:
                break;
            }
        }
    }

    void FPSGameManager::UnlockAllWeaponsForShowcase()
    {
        m_WeaponUnlocked = { true, true, true, true, true };
        m_Ammo = { 999, 60, 150, 30, 80 };
    }

    void FPSGameManager::AimAtNearestVisibleEnemy()
    {
        if (!m_Scene) return;
        glm::vec3 eyePos = m_PlayerFeetPos + glm::vec3(0.0f, m_EyeHeight, 0.0f);
        float bestVisibleDist = 999999.0f;
        glm::vec3 bestTargetPos = eyePos;
        bool foundVisible = false;

        for (const auto& e : m_Scene->GetEntities())
        {
            if (!e.alive || e.tag.tag != "Enemy")
                continue;
            glm::vec3 toEnt = e.transform.position - eyePos;
            float d = glm::length(toEnt);
            if (d < 0.5f) continue;

            glm::vec3 dir = toEnt / d;
            float wallDist = d;
            glm::vec3 hitP, hitN;
            RaycastWorldSolid(eyePos, dir, d, wallDist, hitP, hitN);
            bool hasLOS = (wallDist >= d - 0.85f);

            if (hasLOS && d < bestVisibleDist)
            {
                bestVisibleDist = d;
                bestTargetPos = e.transform.position;
                foundVisible = true;
            }
        }

        if (foundVisible)
        {
            glm::vec3 dir = glm::normalize(bestTargetPos - eyePos);
            m_PlayerYaw   = std::atan2(dir.x, dir.z) * (180.0f / 3.14159265f);
            m_PlayerPitch = std::clamp(-std::asin(std::clamp(dir.y, -1.0f, 1.0f)) * (180.0f / 3.14159265f), -45.0f, 45.0f);
        }
        else
        {
            // Find the longest open corridor sightline from the player's position
            float bestOpenDist = 0.0f;
            float bestYaw = m_PlayerYaw;
            glm::vec3 bestDir(0.0f, 0.0f, 1.0f);
            for (int a = 0; a < 36; ++a)
            {
                float testYaw = a * 10.0f;
                float r = glm::radians(testYaw);
                glm::vec3 d(std::sin(r), 0.0f, std::cos(r));
                float hitDist = 30.0f;
                glm::vec3 hp, hn;
                RaycastWorldSolid(eyePos, d, 30.0f, hitDist, hp, hn);
                if (hitDist > bestOpenDist)
                {
                    bestOpenDist = hitDist;
                    bestYaw = testYaw;
                    bestDir = d;
                }
            }
            m_PlayerYaw = bestYaw;
            m_PlayerPitch = 2.0f;

            // If an enemy exists behind a corner, move one patrol enemy into the open corridor ahead
            if (bestOpenDist > 5.0f)
            {
                for (auto& e : m_Scene->GetEntities())
                {
                    if (e.alive && e.tag.tag == "Enemy")
                    {
                        glm::vec3 targetSpot = m_PlayerFeetPos + bestDir * std::min(bestOpenDist * 0.65f, 10.5f);
                        float fl = m_PlayerFeetPos.y, cl = m_PlayerFeetPos.y + 4.0f;
                        QueryColumnFloorCeil(targetSpot.x, targetSpot.z, m_PlayerFeetPos.y, fl, cl);
                        e.transform.position = glm::vec3(targetSpot.x, fl + 0.85f, targetSpot.z);
                        break;
                    }
                }
            }
        }
    }

    bool FPSGameManager::QueryColumnFloorCeil(float worldX, float worldZ, float currentFeetY, float& outFloorY, float& outCeilY) const
    {
        int cx = static_cast<int>(std::floor(worldX));
        int cy = static_cast<int>(std::floor(worldZ));

        uint64_t key = PackCell(cx, cy);
        auto it = m_ColumnSpans.find(key);
        if (it == m_ColumnSpans.end() || it->second.empty())
            return false;

        // Check diagonal half-solid cell
        auto diagIt = m_DiagonalCorners.find(key);
        if (diagIt != m_DiagonalCorners.end())
        {
            float fx = worldX - std::floor(worldX);
            float fy = worldZ - std::floor(worldZ);
            const std::string& corner = diagIt->second;
            // Solid triangle sits in `corner` ("sw", "se", "nw", "ne")
            if (corner == "sw" && (fx + fy < 0.95f)) return false;
            if (corner == "se" && ((1.0f - fx) + fy < 0.95f)) return false;
            if (corner == "nw" && (fx + (1.0f - fy) < 0.95f)) return false;
            if (corner == "ne" && ((1.0f - fx) + (1.0f - fy) < 0.95f)) return false;
        }

        // Find the span that best matches currentFeetY (supports multi-span room-over-room balconies & stairwells!)
        const BadLevelSpan* bestSpan = nullptr;
        float bestDiff = 999999.0f;
        for (const auto& sp : it->second)
        {
            if (sp.ceilY - sp.floorY < 1.3f)
                continue;
            // Prefer spans whose floor is within step-up (+0.65m) or drop below feet
            float diff = std::abs(sp.floorY - currentFeetY);
            if (currentFeetY >= sp.floorY - 0.65f && currentFeetY + 1.2f <= sp.ceilY + 0.25f)
                diff *= 0.1f;
            if (diff < bestDiff)
            {
                bestDiff = diff;
                bestSpan = &sp;
            }
        }

        if (!bestSpan)
            return false;

        outFloorY = bestSpan->floorY;
        outCeilY  = bestSpan->ceilY;
        return true;
    }

    bool FPSGameManager::CanMoveTo(
        const glm::vec3& fromFeet,
        const glm::vec3& toFeet,
        float radius,
        float height,
        float maxStepUp,
        glm::vec3& outResolvedFeet,
        bool& outGrounded) const
    {
        // Check closed doors first
        if (m_Scene)
        {
            for (const auto& e : m_Scene->GetEntities())
            {
                if (!e.alive || e.tag.tag != "Door")
                    continue;
                if (e.gameplay.state == 2) // Open
                    continue;

                glm::vec3 half = e.collider.boxHalfExtents + glm::vec3(radius, 0.0f, radius);
                glm::vec3 d = glm::abs((toFeet + glm::vec3(0.0f, height * 0.5f, 0.0f)) - e.transform.position);
                if (d.x < half.x && d.y < half.y + height * 0.4f && d.z < half.z)
                    return false;
            }
        }

        // Sample 5 points around the player cylinder footprint
        const glm::vec2 offsets[5] = {
            { 0.0f, 0.0f },
            { radius, 0.0f },
            { -radius, 0.0f },
            { 0.0f, radius },
            { 0.0f, -radius }
        };

        float highestFloor = -99999.0f;
        float lowestCeil   =  99999.0f;

        for (const auto& off : offsets)
        {
            float fl = 0.0f, cl = 0.0f;
            if (!QueryColumnFloorCeil(toFeet.x + off.x, toFeet.z + off.y, fromFeet.y, fl, cl))
                return false;

            highestFloor = std::max(highestFloor, fl);
            lowestCeil   = std::min(lowestCeil, cl);
        }

        // Check step height and ceiling clearance
        if (highestFloor - fromFeet.y > maxStepUp)
            return false;
        if (lowestCeil - std::max(toFeet.y, highestFloor) < height)
            return false;

        outResolvedFeet = toFeet;
        if (outResolvedFeet.y <= highestFloor + 0.08f)
        {
            outResolvedFeet.y = highestFloor;
            outGrounded = true;
        }
        else
        {
            outGrounded = false;
        }
        return true;
    }

    bool FPSGameManager::RaycastWorldSolid(
        const glm::vec3& origin,
        const glm::vec3& dir,
        float maxDist,
        float& outDist,
        glm::vec3& outHitPos,
        glm::vec3& outHitNormal) const
    {
        glm::vec3 rayDir = glm::length(dir) > 0.0001f ? glm::normalize(dir) : glm::vec3(0.0f, 0.0f, 1.0f);
        float step = 0.18f;
        glm::vec3 prevPos = origin;

        for (float t = step; t <= maxDist; t += step)
        {
            glm::vec3 p = origin + rayDir * t;
            float fl = 0.0f, cl = 0.0f;
            if (!QueryColumnFloorCeil(p.x, p.z, p.y - 0.5f, fl, cl) || p.y < fl || p.y > cl)
            {
                outDist = t;
                outHitPos = prevPos;
                if (p.y < fl) outHitNormal = glm::vec3(0.0f, 1.0f, 0.0f);
                else if (p.y > cl) outHitNormal = glm::vec3(0.0f, -1.0f, 0.0f);
                else
                {
                    glm::vec3 diff = prevPos - p;
                    diff.y = 0.0f;
                    outHitNormal = (glm::length(diff) > 0.0001f) ? glm::normalize(diff) : -rayDir;
                }
                return true;
            }
            prevPos = p;
        }

        outDist = maxDist;
        outHitPos = origin + rayDir * maxDist;
        outHitNormal = -rayDir;
        return false;
    }

    void FPSGameManager::SpawnBloodSpatter(const glm::vec3& hitPos, const glm::vec3& hitNormal, int count, bool spawnWallFloorDecals)
    {
        if (!m_Renderer || !m_Scene)
            return;

        int scaledCount = std::max(4, static_cast<int>(count * m_Settings.bloodGoreScale));
        m_Renderer->SpawnParticleBurst(hitPos, glm::vec3(0.85f, 0.05f, 0.04f), scaledCount, 7.5f);
        m_Renderer->SpawnParticleBurst(hitPos, glm::vec3(0.45f, 0.02f, 0.02f), scaledCount / 2, 4.5f);

        if (!spawnWallFloorDecals || m_BloodDecalCount >= m_MaxBloodDecals)
            return;

        // 1. Floor Blood Spatter Pool beneath the hit position
        float fl = 0.0f, cl = 0.0f;
        if (QueryColumnFloorCeil(hitPos.x, hitPos.z, hitPos.y - 0.8f, fl, cl))
        {
            Entity& floorSplat = m_Scene->CreateEntity("BloodFloorDecal", "BloodDecal");
            float jitterX = ((m_BloodDecalCount * 37) % 100 - 50) * 0.006f;
            float jitterZ = ((m_BloodDecalCount * 73) % 100 - 50) * 0.006f;
            float radius  = 0.55f + ((m_BloodDecalCount * 19) % 50) * 0.012f;

            floorSplat.transform.position = glm::vec3(hitPos.x + jitterX, fl + 0.025f, hitPos.z + jitterZ);
            floorSplat.transform.rotation = glm::vec3(0.0f, static_cast<float>((m_BloodDecalCount * 53) % 360), 0.0f);
            floorSplat.transform.scale    = glm::vec3(radius, 0.02f, radius);
            floorSplat.mesh.enabled = true;
            floorSplat.mesh.primitive = MeshPrimitive::Cylinder;
            floorSplat.material.albedoColor = glm::vec4(0.58f, 0.03f, 0.02f, 1.0f);
            floorSplat.material.roughness = 0.18f; // Wet glistening blood
            floorSplat.material.metallic = 0.10f;
            floorSplat.material.patternType = 12;
            ++m_BloodDecalCount;
        }

        // 2. Wall Blood Spatter if near a wall
        float wallDist = 0.0f;
        glm::vec3 wallHitPos, wallNormal;
        glm::vec3 sprayDir = -hitNormal;
        sprayDir.y = 0.0f;
        if (glm::length(sprayDir) < 0.01f)
            sprayDir = glm::vec3(1.0f, 0.0f, 0.0f);

        if (RaycastWorldSolid(hitPos, glm::normalize(sprayDir), 2.8f, wallDist, wallHitPos, wallNormal) && m_BloodDecalCount < m_MaxBloodDecals)
        {
            if (std::abs(wallNormal.y) < 0.5f)
            {
                Entity& wallSplat = m_Scene->CreateEntity("BloodWallDecal", "BloodDecal");
                float splatSize = 0.50f + ((m_BloodDecalCount * 29) % 40) * 0.01f;
                wallSplat.transform.position = wallHitPos + wallNormal * 0.04f;
                wallSplat.transform.scale = ( std::abs(wallNormal.x) > 0.5f )
                    ? glm::vec3(0.03f, splatSize, splatSize)
                    : glm::vec3(splatSize, splatSize, 0.03f);
                wallSplat.mesh.enabled = true;
                wallSplat.mesh.primitive = MeshPrimitive::Cube;
                wallSplat.material.albedoColor = glm::vec4(0.62f, 0.03f, 0.02f, 1.0f);
                wallSplat.material.roughness = 0.22f;
                wallSplat.material.patternType = 12;
                ++m_BloodDecalCount;
            }
        }
    }

    void FPSGameManager::SpawnMeatExplosionGibs(const glm::vec3& pos, int gibCount)
    {
        if (!m_Scene || !m_Renderer)
            return;

        if (m_Audio)
            m_Audio->PlaySound3D("gib_splat", pos, 0.95f, 1.0f);

        SpawnBloodSpatter(pos, glm::vec3(0.0f, 1.0f, 0.0f), 32, true);

        int count = std::max(3, static_cast<int>(gibCount * m_Settings.bloodGoreScale));
        for (int i = 0; i < count; ++i)
        {
            float ang = (static_cast<float>(i) / count) * 2.0f * kPi;
            float spd = 4.5f + (i % 3) * 2.2f;
            glm::vec3 vel(std::cos(ang) * spd, 5.5f + (i % 4) * 1.4f, std::sin(ang) * spd);

            Entity& gib = m_Scene->CreateEntity("MeatGibChunk", "Gib");
            gib.transform.position = pos + glm::vec3(0.0f, 0.25f, 0.0f);
            gib.transform.scale = glm::vec3(0.65f + (i % 3) * 0.15f);
            gib.mesh.enabled = true;
            gib.mesh.primitive = MeshPrimitive::PropGib;
            gib.material.albedoColor = glm::vec4(0.72f, 0.05f, 0.04f, 1.0f);
            gib.material.roughness = 0.25f;

            gib.rigidBody.enabled = true;
            gib.rigidBody.velocity = vel;

            gib.gameplay.enabled = true;
            gib.gameplay.lifetime = 0.0f;
            gib.gameplay.maxLifetime = 4.5f;
        }
    }

    void FPSGameManager::DamageEnemy(Entity& enemy, float damage, const glm::vec3& hitPoint, const glm::vec3& knockbackDir, bool canGib)
    {
        if (!enemy.alive)
            return;

        enemy.gameplay.health -= damage;
        enemy.gameplay.state = 1; // Alert immediately when hit

        SpawnBloodSpatter(hitPoint, -knockbackDir, 16, true);
        if (m_Audio)
            m_Audio->PlaySound3D("hit", hitPoint, 0.75f, 1.0f);

        if (enemy.gameplay.health <= 0.0f)
        {
            glm::vec3 deathPos = enemy.transform.position;
            bool overkill = canGib || (enemy.gameplay.health <= -25.0f);
            enemy.alive = false;
            m_Scene->score += enemy.gameplay.scoreValue;
            ++m_KillsInLevel;
            ++m_Scene->enemiesKilled;
            m_Scene->EmitSignal("enemy_slain", enemy.id, enemy.tag.name);

            if (overkill)
            {
                SpawnMeatExplosionGibs(deathPos, 7);
            }
            else
            {
                SpawnBloodSpatter(deathPos, glm::vec3(0.0f, 1.0f, 0.0f), 26, true);
                if (m_Audio)
                    m_Audio->PlaySound3D("monster_roar", deathPos, 0.85f, 0.65f);
            }
        }
        else
        {
            m_Scene->EmitSignal("enemy_damaged", enemy.id, enemy.tag.name);
        }
    }

    void FPSGameManager::DamagePlayer(float damage, const glm::vec3& sourcePos)
    {
        if (m_PlayerHealth <= 0.0f)
            return;

        float absorbed = std::min(m_PlayerArmor, damage * 0.6f);
        m_PlayerArmor -= absorbed;
        float hpLoss = damage - absorbed;
        m_PlayerHealth = std::max(0.0f, m_PlayerHealth - hpLoss);
        m_DamageFlashAlpha = std::min(0.85f, m_DamageFlashAlpha + 0.45f);

        glm::vec3 eyePos = m_PlayerFeetPos + glm::vec3(0.0f, m_EyeHeight, 0.0f);
        SpawnBloodSpatter(eyePos, glm::normalize(eyePos - sourcePos), 10, true);

        if (m_Audio)
            m_Audio->PlaySound2D("hit", 0.85f, 0.75f);

        if (m_PlayerHealth <= 0.0f)
        {
            m_Scene->gameOver = true;
            SetMenuState(FPSMenuState::GameOver);
            if (m_Audio)
                m_Audio->PlaySound2D("gib_splat", 1.0f, 0.8f);
        }
    }

    void FPSGameManager::ExplodeAt(const glm::vec3& pos, float radius, float maxDamage, EntityID ignoreOwner)
    {
        if (!m_Scene || !m_Renderer)
            return;

        m_Renderer->SpawnParticleBurst(pos, glm::vec3(1.0f, 0.55f, 0.12f), 36, 14.0f);
        m_Renderer->SpawnParticleBurst(pos, glm::vec3(1.0f, 0.85f, 0.25f), 20, 9.0f);
        if (m_Audio)
            m_Audio->PlaySound3D("explosion", pos, 1.0f, 0.95f);

        for (auto& e : m_Scene->GetEntities())
        {
            if (!e.alive || e.id == ignoreOwner)
                continue;

            if (e.tag.tag == "Enemy")
            {
                float d = glm::length(e.transform.position - pos);
                if (d < radius)
                {
                    float dmg = maxDamage * (1.0f - (d / radius));
                    glm::vec3 dir = (d > 0.01f) ? glm::normalize(e.transform.position - pos) : glm::vec3(0.0f, 1.0f, 0.0f);
                    DamageEnemy(e, dmg, e.transform.position, dir, true);
                }
            }
            else if (e.tag.tag == "Barrel")
            {
                float d = glm::length(e.transform.position - pos);
                if (d < radius && d > 0.1f)
                {
                    e.alive = false;
                    ExplodeAt(e.transform.position, 6.5f, 85.0f, e.id);
                }
            }
        }
    }

    void FPSGameManager::FireCurrentWeapon()
    {
        if (!m_Scene || !m_Renderer || m_WeaponFireTimer > 0.0f)
            return;

        int wIdx = static_cast<int>(m_CurrentWeapon);
        if (wIdx > 0 && m_Ammo[wIdx] <= 0)
        {
            // Auto-switch to best weapon with ammo
            for (int k = 4; k >= 0; --k)
            {
                if (m_WeaponUnlocked[k] && m_Ammo[k] > 0)
                {
                    SelectWeapon(k);
                    wIdx = k;
                    break;
                }
            }
        }

        glm::vec3 eyePos = m_PlayerFeetPos + glm::vec3(0.0f, m_EyeHeight, 0.0f);
        float yawRad   = glm::radians(m_PlayerYaw);
        float pitchRad = glm::radians(m_PlayerPitch);
        glm::vec3 aimDir = glm::normalize(glm::vec3(
            std::sin(yawRad) * std::cos(pitchRad),
            -std::sin(pitchRad),
            std::cos(yawRad) * std::cos(pitchRad)));
        glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), aimDir));
        glm::vec3 up    = glm::normalize(glm::cross(aimDir, right));

        auto raycastHitscan = [&](const glm::vec3& dir, float range, float damage, bool isMelee) {
            float wallDist = range;
            glm::vec3 wallHit, wallNorm;
            RaycastWorldSolid(eyePos, dir, range, wallDist, wallHit, wallNorm);

            Entity* bestEnemy = nullptr;
            float bestDist = wallDist;
            glm::vec3 bestHitPoint = wallHit;

            for (auto& e : m_Scene->GetEntities())
            {
                if (!e.alive) continue;
                if (e.tag.tag != "Enemy" && e.tag.tag != "Barrel" && e.tag.tag != "Door")
                    continue;

                glm::vec3 toEnt = e.transform.position - eyePos;
                float proj = glm::dot(toEnt, dir);
                if (proj < 0.0f || proj > bestDist)
                    continue;

                glm::vec3 closest = eyePos + dir * proj;
                float hitRadius = (e.tag.tag == "Door") ? 1.4f : (e.collider.sphereRadius + 0.35f);
                if (glm::length(closest - e.transform.position) <= hitRadius)
                {
                    bestDist = proj;
                    bestEnemy = &e;
                    bestHitPoint = closest;
                }
            }

            if (bestEnemy)
            {
                if (bestEnemy->tag.tag == "Enemy")
                {
                    DamageEnemy(*bestEnemy, damage, bestHitPoint, dir, damage >= 60.0f);
                }
                else if (bestEnemy->tag.tag == "Barrel")
                {
                    bestEnemy->gameplay.health -= damage;
                    if (bestEnemy->gameplay.health <= 0.0f)
                    {
                        glm::vec3 bPos = bestEnemy->transform.position;
                        bestEnemy->alive = false;
                        ExplodeAt(bPos, 7.0f, 95.0f, bestEnemy->id);
                    }
                }
                else if (bestEnemy->tag.tag == "Door" && bestEnemy->gameplay.subType == 1 && bestEnemy->gameplay.state == 0)
                {
                    // Shooting or striking a secret door opens it!
                    bestEnemy->gameplay.state = 1;
                    ++m_SecretsFound;
                    m_StatusMessage = "YOU FOUND A SECRET AREA!";
                    m_StatusMessageTimer = 3.5f;
                    if (m_Audio) m_Audio->PlaySound3D("door_open", bestEnemy->transform.position, 0.9f, 1.0f);
                }
            }
            else if (!isMelee && bestDist < range)
            {
                m_Renderer->SpawnParticleBurst(wallHit, glm::vec3(0.95f, 0.78f, 0.35f), 5, 4.0f);
            }
        };

        switch (m_CurrentWeapon)
        {
        case FPSWeaponType::RunicAxe:
        {
            m_WeaponFireTimer = 0.36f;
            m_WeaponRecoil = 0.32f;
            if (m_Audio) m_Audio->PlaySound2D("axe_swing", 0.85f, 1.0f);
            raycastHitscan(aimDir, 3.2f, 55.0f, true);
            break;
        }
        case FPSWeaponType::Shotgun:
        {
            --m_Ammo[1];
            m_WeaponFireTimer = 0.52f;
            m_WeaponRecoil = 0.38f;
            m_MuzzleFlashTimer = 0.09f;
            if (m_Audio) m_Audio->PlaySound2D("shotgun", 0.90f, 1.0f);

            // 8 buckshot pellets in a tight spread
            const glm::vec2 spread[8] = {
                { 0.0f, 0.0f }, { -0.03f, 0.01f }, { 0.03f, -0.01f }, { -0.015f, -0.025f },
                { 0.015f, 0.025f }, { -0.045f, -0.01f }, { 0.045f, 0.01f }, { 0.0f, 0.035f }
            };
            for (int p = 0; p < 8; ++p)
            {
                glm::vec3 pDir = glm::normalize(aimDir + right * spread[p].x + up * spread[p].y);
                raycastHitscan(pDir, 45.0f, 11.0f, false);
            }
            break;
        }
        case FPSWeaponType::SuperNailgun:
        {
            --m_Ammo[2];
            m_WeaponFireTimer = 0.11f;
            m_WeaponRecoil = 0.16f;
            m_MuzzleFlashTimer = 0.05f;
            if (m_Audio) m_Audio->PlaySound2D("nailgun", 0.80f, 1.0f);

            Entity& nail = m_Scene->CreateEntity("SuperNailSpike", "Projectile");
            nail.transform.position = eyePos + aimDir * 0.65f + right * 0.12f - up * 0.10f;
            nail.transform.rotation = glm::vec3(m_PlayerPitch, m_PlayerYaw, 0.0f);
            nail.transform.scale    = glm::vec3(0.08f, 0.08f, 0.55f);
            nail.mesh.enabled = true;
            nail.mesh.primitive = MeshPrimitive::Cube;
            nail.material.albedoColor = glm::vec4(0.95f, 0.85f, 0.45f, 1.0f);
            nail.material.emissiveColor = glm::vec3(1.0f, 0.75f, 0.25f);
            nail.material.emissiveStrength = 3.0f;
            nail.rigidBody.enabled = true;
            nail.rigidBody.velocity = aimDir * 36.0f;
            nail.gameplay.enabled = true;
            nail.gameplay.damage = 24.0f;
            nail.gameplay.maxLifetime = 2.5f;
            nail.gameplay.subType = 0; // Nail
            break;
        }
        case FPSWeaponType::RocketLauncher:
        {
            --m_Ammo[3];
            m_WeaponFireTimer = 0.72f;
            m_WeaponRecoil = 0.48f;
            m_MuzzleFlashTimer = 0.14f;
            if (m_Audio) m_Audio->PlaySound2D("rocket_fire", 0.92f, 1.0f);

            Entity& rocket = m_Scene->CreateEntity("OgreRocket", "Projectile");
            rocket.transform.position = eyePos + aimDir * 0.75f + right * 0.14f - up * 0.10f;
            rocket.transform.rotation = glm::vec3(m_PlayerPitch, m_PlayerYaw, 0.0f);
            rocket.transform.scale    = glm::vec3(0.24f, 0.24f, 0.60f);
            rocket.mesh.enabled = true;
            rocket.mesh.primitive = MeshPrimitive::Sphere;
            rocket.material.albedoColor = glm::vec4(1.0f, 0.55f, 0.15f, 1.0f);
            rocket.material.emissiveColor = glm::vec3(1.0f, 0.45f, 0.10f);
            rocket.material.emissiveStrength = 4.5f;
            rocket.light.enabled = true;
            rocket.light.color = glm::vec3(1.0f, 0.55f, 0.15f);
            rocket.light.intensity = 4.5f;
            rocket.light.range = 12.0f;
            rocket.rigidBody.enabled = true;
            rocket.rigidBody.velocity = aimDir * 24.0f;
            rocket.gameplay.enabled = true;
            rocket.gameplay.damage = 110.0f;
            rocket.gameplay.maxLifetime = 4.0f;
            rocket.gameplay.subType = 1; // Explosive Rocket
            break;
        }
        case FPSWeaponType::LightningGun:
        {
            --m_Ammo[4];
            m_WeaponFireTimer = 0.09f;
            m_WeaponRecoil = 0.12f;
            m_MuzzleFlashTimer = 0.06f;
            if (m_Audio) m_Audio->PlaySound2D("lightning", 0.80f, 1.0f);

            // Render crackling cyan-white 3D lightning arc from gun barrel to target
            glm::vec3 startBeam = eyePos + aimDir * 0.55f + right * 0.18f - up * 0.14f;
            glm::vec3 endBeam   = eyePos + aimDir * 22.0f;
            float hitD = 22.0f;
            glm::vec3 hitP, hitN;
            if (RaycastWorldSolid(eyePos, aimDir, 22.0f, hitD, hitP, hitN))
                endBeam = hitP;

            int segs = 8;
            glm::vec3 prev = startBeam;
            for (int s = 1; s <= segs; ++s)
            {
                float t = static_cast<float>(s) / segs;
                glm::vec3 pt = glm::mix(startBeam, endBeam, t);
                if (s < segs)
                {
                    float jx = std::sin(m_LevelTime * 45.0f + s * 3.1f) * 0.14f;
                    float jy = std::cos(m_LevelTime * 53.0f + s * 2.3f) * 0.14f;
                    pt += right * jx + up * jy;
                }
                m_Renderer->AddTransientLine(prev, pt, glm::vec4(0.45f, 0.95f, 1.0f, 1.0f));
                m_Renderer->AddTransientLine(prev + up * 0.02f, pt - up * 0.02f, glm::vec4(0.90f, 0.98f, 1.0f, 1.0f));
                prev = pt;
            }

            raycastHitscan(aimDir, 22.0f, 34.0f, false);
            break;
        }
        default:
            break;
        }
    }

    void FPSGameManager::SetPlayerPose(const glm::vec3& feetPos, float yawDeg, float pitchDeg)
    {
        m_PlayerFeetPos = feetPos;
        m_PlayerYaw = yawDeg;
        m_PlayerPitch = pitchDeg;
    }

    void FPSGameManager::UpdateFirstPersonCamera(CameraView& ioCamera, float dt)
    {
        (void)dt;
        float bobY = std::sin(m_HeadBobPhase) * 0.055f;
        float bobX = std::cos(m_HeadBobPhase * 0.5f) * 0.03f;

        float yawRad   = glm::radians(m_PlayerYaw);
        float pitchRad = glm::radians(m_PlayerPitch);

        glm::vec3 forward = glm::normalize(glm::vec3(
            std::sin(yawRad) * std::cos(pitchRad),
            -std::sin(pitchRad),
            std::cos(yawRad) * std::cos(pitchRad)));
        glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), forward));
        glm::vec3 up    = glm::normalize(glm::cross(forward, right));

        ioCamera.position   = m_PlayerFeetPos + glm::vec3(0.0f, m_EyeHeight + bobY, 0.0f) + right * bobX;
        ioCamera.forward    = forward;
        ioCamera.up         = glm::vec3(0.0f, 1.0f, 0.0f);
        ioCamera.fovDegrees = m_Settings.fovDegrees;
        ioCamera.nearClip   = 0.08f;
        ioCamera.farClip    = 280.0f;

        // Position 3D First-Person Weapon ViewModel in front of the camera
        if (Entity* vm = m_Scene ? m_Scene->GetEntity(m_ViewModelEntity) : nullptr)
        {
            vm->mesh.enabled = !IsInMenu();
            float swayX = std::sin(m_WeaponSwayPhase) * 0.018f;
            float swayY = std::abs(std::cos(m_WeaponSwayPhase)) * 0.015f;
            float recoilZ = m_WeaponRecoil * 0.16f;
            float recoilPitch = -m_WeaponRecoil * 18.0f;

            glm::vec3 vmPos = ioCamera.position
                + forward * (0.52f - recoilZ)
                + right * (0.24f + swayX)
                - up * (0.22f + swayY);

            vm->transform.position = vmPos;
            vm->transform.rotation = glm::vec3(m_PlayerPitch + recoilPitch, m_PlayerYaw, 0.0f);
            vm->transform.scale    = glm::vec3(0.48f);
        }

        // Update Player Torch / Muzzle Flash Light
        if (Entity* pl = m_Scene ? m_Scene->GetEntity(m_PlayerLightEntity) : nullptr)
        {
            pl->transform.position = ioCamera.position + forward * 0.6f;
            if (m_MuzzleFlashTimer > 0.0f)
            {
                pl->light.enabled = true;
                pl->light.color = (m_CurrentWeapon == FPSWeaponType::LightningGun)
                    ? glm::vec3(0.35f, 0.92f, 1.0f)
                    : glm::vec3(1.0f, 0.72f, 0.25f);
                pl->light.intensity = 6.5f;
                pl->light.range = 20.0f;
            }
            else
            {
                pl->light.enabled = m_Settings.dynamicLights;
                pl->light.color = glm::vec3(1.0f, 0.85f, 0.68f);
                pl->light.intensity = 1.6f * m_Settings.brightness;
                pl->light.range = 14.0f;
            }
        }
    }
}

namespace Hargon
{
    using json = nlohmann::json;

    void FPSGameManager::Step(float dt)
    {
        if (!m_Scene || !m_Renderer)
            return;

        // Handle ESC pause toggle & F5 / F9 QuickSave / QuickLoad
        if (m_Window)
        {
            if (m_Window->IsKeyPressed(GLFW_KEY_ESCAPE))
            {
                if (m_MenuState == FPSMenuState::None)
                    SetMenuState(FPSMenuState::PauseMenu);
                else if (m_MenuState == FPSMenuState::PauseMenu)
                    SetMenuState(FPSMenuState::None);
                else if (m_MenuState == FPSMenuState::SettingsMenu || m_MenuState == FPSMenuState::SaveLoadMenu || m_MenuState == FPSMenuState::CampaignSelect)
                    SetMenuState(m_PreviousMenuState);
            }

            if (m_Window->IsKeyPressed(GLFW_KEY_F5))
            {
                std::string msg;
                if (SaveGame(0, &msg))
                {
                    m_StatusMessage = "QUICKSAVED GAME (SLOT 0)";
                    m_StatusMessageTimer = 3.0f;
                }
            }
            if (m_Window->IsKeyPressed(GLFW_KEY_F9))
            {
                std::string msg;
                if (LoadGame(0, &msg))
                {
                    m_StatusMessage = "QUICKLOADED GAME (SLOT 0)";
                    m_StatusMessageTimer = 3.0f;
                    SetMenuState(FPSMenuState::None);
                }
            }
        }

        if (m_StatusMessageTimer > 0.0f)
            m_StatusMessageTimer = std::max(0.0f, m_StatusMessageTimer - dt);
        if (m_DamageFlashAlpha > 0.0f)
            m_DamageFlashAlpha = std::max(0.0f, m_DamageFlashAlpha - dt * 1.8f);
        if (m_PickupFlashAlpha > 0.0f)
            m_PickupFlashAlpha = std::max(0.0f, m_PickupFlashAlpha - dt * 2.2f);

        if (IsInMenu())
        {
            m_Renderer->UpdateParticles(dt);
            return;
        }

        m_LevelTime += dt;
        TweenSystem::Get().Step(*m_Scene, dt);
        m_WeaponFireTimer  = std::max(0.0f, m_WeaponFireTimer - dt);
        m_MuzzleFlashTimer = std::max(0.0f, m_MuzzleFlashTimer - dt);
        m_WeaponRecoil     = std::max(0.0f, m_WeaponRecoil - dt * 2.2f);

        // 1. Player Mouse Look & Keyboard Turning + Weapon Selection (1..5 or Scroll Wheel)
        if (m_Window)
        {
            if (m_Window->IsCursorCaptured() || m_Window->IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
            {
                glm::vec2 md = m_Window->GetMouseDelta();
                m_PlayerYaw   += md.x * m_Settings.mouseSensitivity;
                m_PlayerPitch  = std::clamp(m_PlayerPitch + md.y * m_Settings.mouseSensitivity, -82.0f, 82.0f);
            }

            // Keyboard arrow turning for accessibility / laptop play
            float turnSpeed = 125.0f * dt;
            if (m_Window->IsKeyDown(GLFW_KEY_LEFT))  m_PlayerYaw -= turnSpeed;
            if (m_Window->IsKeyDown(GLFW_KEY_RIGHT)) m_PlayerYaw += turnSpeed;
            if (m_Window->IsKeyDown(GLFW_KEY_UP))    m_PlayerPitch = std::clamp(m_PlayerPitch - turnSpeed * 0.7f, -82.0f, 82.0f);
            if (m_Window->IsKeyDown(GLFW_KEY_DOWN))  m_PlayerPitch = std::clamp(m_PlayerPitch + turnSpeed * 0.7f, -82.0f, 82.0f);

            // Weapon hotkeys 1..5
            for (int w = 0; w < 5; ++w)
            {
                if (m_Window->IsKeyPressed(GLFW_KEY_1 + w))
                    SelectWeapon(w);
            }

            float scroll = m_Window->GetScrollDelta();
            if (std::abs(scroll) > 0.1f)
            {
                int dir = (scroll > 0.0f) ? 1 : -1;
                int cur = static_cast<int>(m_CurrentWeapon);
                for (int tries = 1; tries <= 5; ++tries)
                {
                    int nextW = (cur + dir * tries + 50) % 5;
                    if (m_WeaponUnlocked[nextW])
                    {
                        SelectWeapon(nextW);
                        break;
                    }
                }
            }

            // 2. First-Person WASD Movement, Stair Step-Up, and Jumping
            float yawRad = glm::radians(m_PlayerYaw);
            glm::vec3 flatForward(std::sin(yawRad), 0.0f, std::cos(yawRad));
            glm::vec3 flatRight(flatForward.z, 0.0f, -flatForward.x);

            glm::vec3 moveInput(0.0f);
            glm::vec2 actionVec = InputMap::Get().GetVector("move_left", "move_right", "move_backward", "move_forward", m_Window);
            moveInput = flatForward * actionVec.y + flatRight * actionVec.x;

            float moveSpeed = InputMap::Get().IsActionPressed("sprint", m_Window) ? 12.0f : 8.8f;
            if (glm::length(moveInput) > 0.01f)
            {
                moveInput = glm::normalize(moveInput) * moveSpeed;
                m_HeadBobPhase   += dt * 11.5f;
                m_WeaponSwayPhase += dt * 7.5f;
            }
            else
            {
                m_WeaponSwayPhase += dt * 1.8f;
            }

            // Jump via InputMap ("jump" action)
            if (InputMap::Get().IsActionJustPressed("jump", m_Window) && m_PlayerGrounded)
            {
                m_PlayerVelocity.y = 6.8f;
                m_PlayerGrounded = false;
            }

            m_PlayerVelocity.y -= 17.5f * dt; // Gravity
            glm::vec3 desiredDelta(moveInput.x * dt, m_PlayerVelocity.y * dt, moveInput.z * dt);

            // Slide collision (X, then Z, then Y) with 0.62m step-up for Quake 16-unit stairs!
            const float pRadius = 0.34f;
            const float pHeight = 1.65f;
            const float maxStep = 0.62f;

            glm::vec3 resolved = m_PlayerFeetPos;
            bool grounded = m_PlayerGrounded;

            // Try full XZ move first
            if (CanMoveTo(m_PlayerFeetPos, m_PlayerFeetPos + glm::vec3(desiredDelta.x, 0.0f, desiredDelta.z), pRadius, pHeight, maxStep, resolved, grounded))
            {
                m_PlayerFeetPos.x = resolved.x;
                m_PlayerFeetPos.z = resolved.z;
                if (resolved.y > m_PlayerFeetPos.y)
                    m_PlayerFeetPos.y = resolved.y; // Smooth step up stairs
            }
            else
            {
                // Slide X
                if (CanMoveTo(m_PlayerFeetPos, m_PlayerFeetPos + glm::vec3(desiredDelta.x, 0.0f, 0.0f), pRadius, pHeight, maxStep, resolved, grounded))
                {
                    m_PlayerFeetPos.x = resolved.x;
                    if (resolved.y > m_PlayerFeetPos.y) m_PlayerFeetPos.y = resolved.y;
                }
                // Slide Z
                if (CanMoveTo(m_PlayerFeetPos, m_PlayerFeetPos + glm::vec3(0.0f, 0.0f, desiredDelta.z), pRadius, pHeight, maxStep, resolved, grounded))
                {
                    m_PlayerFeetPos.z = resolved.z;
                    if (resolved.y > m_PlayerFeetPos.y) m_PlayerFeetPos.y = resolved.y;
                }
            }

            // Vertical step
            float fl = m_PlayerFeetPos.y, cl = m_PlayerFeetPos.y + 4.0f;
            if (QueryColumnFloorCeil(m_PlayerFeetPos.x, m_PlayerFeetPos.z, m_PlayerFeetPos.y, fl, cl))
            {
                m_PlayerFeetPos.y += desiredDelta.y;
                if (m_PlayerFeetPos.y <= fl + 0.04f)
                {
                    m_PlayerFeetPos.y = fl;
                    m_PlayerVelocity.y = 0.0f;
                    m_PlayerGrounded = true;
                }
                else if (m_PlayerFeetPos.y + pHeight >= cl)
                {
                    m_PlayerFeetPos.y = cl - pHeight;
                    m_PlayerVelocity.y = std::min(0.0f, m_PlayerVelocity.y);
                }
            }

            // Fire weapon with Left Mouse Button, F, or Enter
            if (m_Window->IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT) || m_Window->IsKeyDown(GLFW_KEY_F) || m_Window->IsKeyDown(GLFW_KEY_ENTER))
            {
                FireCurrentWeapon();
            }
        }

        if (m_Audio)
        {
            float yawRad = glm::radians(m_PlayerYaw);
            m_Audio->SetListenerTransform(
                m_PlayerFeetPos + glm::vec3(0.0f, m_EyeHeight, 0.0f),
                glm::vec3(std::sin(yawRad), 0.0f, std::cos(yawRad)));
        }

        // 3. Step Doors, Lifts, Pickups, Projectiles, Gibs, and all 10 Enemy AI Types
        glm::vec3 playerCenter = m_PlayerFeetPos + glm::vec3(0.0f, 0.9f, 0.0f);

        for (size_t i = 0; i < m_Scene->GetEntities().size(); ++i)
        {
            Entity& e = m_Scene->GetEntities()[i];
            if (!e.alive)
                continue;

            // A) Doors (Locked / Unlocked / Secret)
            if (e.tag.tag == "Door")
            {
                float dist = glm::length(e.gameplay.basePos - playerCenter);
                if (e.gameplay.state == 0 && dist < 3.4f)
                {
                    const std::string& reqKey = e.gameplay.extraKey;
                    bool canOpen = false;
                    if (reqKey.empty())
                        canOpen = true;
                    else if (reqKey == "red" && m_HasRedKey)
                        canOpen = true;
                    else if (reqKey == "blue" && m_HasBlueKey)
                        canOpen = true;
                    else if (e.gameplay.subType == 1) // Secret door touched by player
                    {
                        canOpen = true;
                        ++m_SecretsFound;
                        m_StatusMessage = "SECRET AREA DISCOVERED!";
                        m_StatusMessageTimer = 3.5f;
                    }
                    else
                    {
                        m_StatusMessage = (reqKey == "red") ? "YOU NEED THE RED RUNIC KEY!" : "YOU NEED THE BLUE RUNIC KEY!";
                        m_StatusMessageTimer = 2.0f;
                    }

                    if (canOpen)
                    {
                        e.gameplay.state = 1;
                        if (m_Audio) m_Audio->PlaySound3D("door_open", e.transform.position, 0.9f, 1.0f);
                    }
                }
                else if (e.gameplay.state == 1)
                {
                    e.transform.position = glm::mix(e.transform.position, e.gameplay.targetPos, std::min(1.0f, dt * 3.5f));
                    if (glm::length(e.transform.position - e.gameplay.targetPos) < 0.08f)
                    {
                        e.transform.position = e.gameplay.targetPos;
                        e.gameplay.state = 2; // Open
                    }
                }
            }
            // B) Elevator Lifts
            else if (e.tag.tag == "Lift")
            {
                glm::vec2 flatDiff(m_PlayerFeetPos.x - e.transform.position.x, m_PlayerFeetPos.z - e.transform.position.z);
                if (glm::length(flatDiff) < 1.8f && std::abs(m_PlayerFeetPos.y - e.gameplay.basePos.y) < 3.5f)
                {
                    e.gameplay.state = 1; // Rise to top landing
                }
                if (e.gameplay.state == 1)
                {
                    float oldY = e.transform.position.y;
                    e.transform.position.y = std::min(e.gameplay.targetPos.y, e.transform.position.y + 3.5f * dt);
                    float dy = e.transform.position.y - oldY;
                    if (glm::length(flatDiff) < 1.6f)
                        m_PlayerFeetPos.y += dy;
                    if (e.transform.position.y >= e.gameplay.targetPos.y - 0.02f)
                        e.gameplay.state = 2;
                }
            }
            // C) Exit Slipgate
            else if (e.tag.tag == "Exit")
            {
                e.transform.rotation.y += 45.0f * dt;
                if (glm::length(e.transform.position - playerCenter) < 1.85f)
                {
                    if (m_Audio) m_Audio->PlaySound2D("victory", 0.95f, 1.0f);
                    if (m_LevelIndex < 5)
                        SetMenuState(FPSMenuState::LevelComplete);
                    else
                        SetMenuState(FPSMenuState::CampaignVictory);
                    return;
                }
            }
            // D) Pickups (Weapons, Ammo, Health, MegaHealth, Armor, Keys)
            else if (e.tag.tag == "Pickup")
            {
                e.transform.rotation.y += 95.0f * dt;
                e.transform.position.y = e.gameplay.basePos.y + std::sin(m_LevelTime * 3.2f + e.id) * 0.10f;

                if (glm::length(e.transform.position - playerCenter) < 1.45f)
                {
                    FPSPickupType pType = static_cast<FPSPickupType>(e.gameplay.subType);
                    bool consumed = true;

                    switch (pType)
                    {
                    case FPSPickupType::Health:
                        if (m_PlayerHealth >= m_PlayerMaxHealth) consumed = false;
                        else
                        {
                            m_PlayerHealth = std::min(m_PlayerMaxHealth, m_PlayerHealth + 25.0f);
                            m_StatusMessage = "PICKED UP MEDKIT (+25 HEALTH)";
                        }
                        break;
                    case FPSPickupType::MegaHealth:
                        m_PlayerHealth = std::min(200.0f, m_PlayerHealth + 100.0f);
                        m_StatusMessage = "PICKED UP RUNIC MEGAHEALTH (+100 HEALTH)!";
                        break;
                    case FPSPickupType::Armor:
                        m_PlayerArmor = std::min(m_PlayerMaxArmor, m_PlayerArmor + 50.0f);
                        m_StatusMessage = "PICKED UP ELDER ARMOR (+50 ARMOR)";
                        break;
                    case FPSPickupType::AmmoShells:
                        m_Ammo[1] = std::min(m_MaxAmmo[1], m_Ammo[1] + 20);
                        m_StatusMessage = "PICKED UP BUCKSHOT SHELLS (+20)";
                        break;
                    case FPSPickupType::AmmoNails:
                        m_Ammo[2] = std::min(m_MaxAmmo[2], m_Ammo[2] + 45);
                        m_StatusMessage = "PICKED UP PERFORATOR NAILS (+45)";
                        break;
                    case FPSPickupType::AmmoRockets:
                        m_Ammo[3] = std::min(m_MaxAmmo[3], m_Ammo[3] + 8);
                        m_StatusMessage = "PICKED UP OGRE ROCKETS (+8)";
                        break;
                    case FPSPickupType::AmmoCells:
                        m_Ammo[4] = std::min(m_MaxAmmo[4], m_Ammo[4] + 25);
                        m_StatusMessage = "PICKED UP THUNDERBOLT CELLS (+25)";
                        break;
                    case FPSPickupType::WeaponShotgun:
                        m_WeaponUnlocked[1] = true;
                        m_Ammo[1] = std::min(m_MaxAmmo[1], m_Ammo[1] + 16);
                        SelectWeapon(1);
                        m_StatusMessage = "ACQUIRED DOUBLE-BARREL SHOTGUN!";
                        break;
                    case FPSPickupType::WeaponSuperNailgun:
                        m_WeaponUnlocked[2] = true;
                        m_Ammo[2] = std::min(m_MaxAmmo[2], m_Ammo[2] + 50);
                        SelectWeapon(2);
                        m_StatusMessage = "ACQUIRED SUPER PERFORATOR NAILGUN!";
                        break;
                    case FPSPickupType::WeaponRocketLauncher:
                        m_WeaponUnlocked[3] = true;
                        m_Ammo[3] = std::min(m_MaxAmmo[3], m_Ammo[3] + 10);
                        SelectWeapon(3);
                        m_StatusMessage = "ACQUIRED OGRE ROCKET LAUNCHER!";
                        break;
                    case FPSPickupType::WeaponLightningGun:
                        m_WeaponUnlocked[4] = true;
                        m_Ammo[4] = std::min(m_MaxAmmo[4], m_Ammo[4] + 35);
                        SelectWeapon(4);
                        m_StatusMessage = "ACQUIRED THUNDERBOLT LIGHTNING SHAFT!";
                        break;
                    case FPSPickupType::KeyRed:
                        m_HasRedKey = true;
                        m_StatusMessage = "ACQUIRED THE RED RUNIC KEY!";
                        break;
                    case FPSPickupType::KeyBlue:
                        m_HasBlueKey = true;
                        m_StatusMessage = "ACQUIRED THE BLUE RUNIC KEY!";
                        break;
                    }

                    if (consumed)
                    {
                        e.alive = false;
                        m_PickupFlashAlpha = 0.35f;
                        m_StatusMessageTimer = 3.0f;
                        if (m_Audio) m_Audio->PlaySound2D("pickup", 0.85f, 1.0f);
                    }
                }
            }
            // E) Bouncing Bloody Gib Chunks
            else if (e.tag.tag == "Gib")
            {
                e.gameplay.lifetime += dt;
                if (e.gameplay.lifetime >= e.gameplay.maxLifetime)
                {
                    e.alive = false;
                    continue;
                }
                e.rigidBody.velocity.y -= 16.0f * dt;
                e.transform.position += e.rigidBody.velocity * dt;
                e.transform.rotation += glm::vec3(180.0f, 240.0f, 120.0f) * dt;

                float fl = 0.0f, cl = 4.0f;
                if (QueryColumnFloorCeil(e.transform.position.x, e.transform.position.z, e.transform.position.y, fl, cl))
                {
                    if (e.transform.position.y <= fl + 0.15f)
                    {
                        e.transform.position.y = fl + 0.15f;
                        e.rigidBody.velocity.y = std::abs(e.rigidBody.velocity.y) * 0.42f;
                        e.rigidBody.velocity.x *= 0.72f;
                        e.rigidBody.velocity.z *= 0.72f;
                        if (std::abs(e.rigidBody.velocity.y) > 1.2f)
                            SpawnBloodSpatter(e.transform.position, glm::vec3(0.0f, 1.0f, 0.0f), 4, true);
                    }
                }
            }
            // F) Player & Enemy Projectiles (Nails, Rockets, Ogre Grenades, Hell Knight Bolts, Scrag Acid, Vore Homing Pods)
            else if (e.tag.tag == "Projectile" || e.tag.tag == "EnemyProjectile")
            {
                e.gameplay.lifetime += dt;
                if (e.gameplay.lifetime >= e.gameplay.maxLifetime)
                {
                    if (e.tag.tag == "Projectile" && e.gameplay.subType == 1)
                        ExplodeAt(e.transform.position, 6.0f, e.gameplay.damage, e.id);
                    e.alive = false;
                    continue;
                }

                // Vore Homing Pod steers gently toward player
                if (e.tag.tag == "EnemyProjectile" && e.gameplay.subType == 2)
                {
                    glm::vec3 toPlayer = glm::normalize(playerCenter - e.transform.position);
                    e.rigidBody.velocity = glm::normalize(glm::mix( glm::normalize(e.rigidBody.velocity), toPlayer, dt * 2.5f )) * 13.5f;
                }
                // Ogre Bouncing Grenade has gravity
                else if (e.tag.tag == "EnemyProjectile" && e.gameplay.subType == 1)
                {
                    e.rigidBody.velocity.y -= 12.0f * dt;
                }

                e.transform.position += e.rigidBody.velocity * dt;

                // Check world collision
                float fl = 0.0f, cl = 4.0f;
                bool hitWall = !QueryColumnFloorCeil(e.transform.position.x, e.transform.position.z, e.transform.position.y - 0.3f, fl, cl)
                    || e.transform.position.y <= fl + 0.1f
                    || e.transform.position.y >= cl - 0.1f;

                if (hitWall)
                {
                    if (e.tag.tag == "EnemyProjectile" && e.gameplay.subType == 1 && e.transform.position.y <= fl + 0.25f)
                    {
                        // Ogre grenade bounces off the floor!
                        e.transform.position.y = fl + 0.25f;
                        e.rigidBody.velocity.y = std::abs(e.rigidBody.velocity.y) * 0.65f;
                    }
                    else
                    {
                        if (e.tag.tag == "Projectile" && e.gameplay.subType == 1)
                            ExplodeAt(e.transform.position, 6.0f, e.gameplay.damage, e.id);
                        else
                            m_Renderer->SpawnParticleBurst(e.transform.position, glm::vec3(1.0f, 0.65f, 0.20f), 8, 5.0f);
                        e.alive = false;
                        continue;
                    }
                }

                if (e.tag.tag == "Projectile")
                {
                    for (auto& target : m_Scene->GetEntities())
                    {
                        if (!target.alive || (target.tag.tag != "Enemy" && target.tag.tag != "Barrel"))
                            continue;
                        if (glm::length(target.transform.position - e.transform.position) < target.collider.sphereRadius + 0.35f)
                        {
                            if (e.gameplay.subType == 1) // Rocket splash explosion
                            {
                                ExplodeAt(e.transform.position, 6.2f, e.gameplay.damage, e.id);
                            }
                            else if (target.tag.tag == "Enemy")
                            {
                                DamageEnemy(target, e.gameplay.damage, e.transform.position, glm::normalize(e.rigidBody.velocity), false);
                            }
                            else if (target.tag.tag == "Barrel")
                            {
                                target.alive = false;
                                ExplodeAt(target.transform.position, 6.5f, 90.0f, target.id);
                            }
                            e.alive = false;
                            break;
                        }
                    }
                }
                else // EnemyProjectile hitting Player
                {
                    if (glm::length(e.transform.position - playerCenter) < 0.85f)
                    {
                        DamagePlayer(e.gameplay.damage, e.transform.position);
                        e.alive = false;
                    }
                }
            }
            // G) 10 Distinct Enemy AI Behaviors
            else if (e.tag.tag == "Enemy")
            {
                FPSEnemyType eType = static_cast<FPSEnemyType>(e.gameplay.subType);
                glm::vec3 toPlayer = playerCenter - e.transform.position;
                float distToPlayer = glm::length(toPlayer);

                // Alert check: within 22m and line of sight (or within 8m hearing range)
                if (e.gameplay.state == 0 && distToPlayer < 22.0f)
                {
                    float wallDist = 0.0f;
                    glm::vec3 hPos, hNorm;
                    if (distToPlayer < 8.0f || !RaycastWorldSolid(e.transform.position, toPlayer / std::max(distToPlayer, 0.01f), distToPlayer, wallDist, hPos, hNorm))
                    {
                        e.gameplay.state = 1;
                        if (m_Audio) m_Audio->PlaySound3D("monster_roar", e.transform.position, 0.75f, 0.9f + (e.gameplay.subType % 3) * 0.1f);
                    }
                }

                if (e.gameplay.state == 1 && distToPlayer > 0.1f)
                {
                    glm::vec3 dirFlat(toPlayer.x, 0.0f, toPlayer.z);
                    if (glm::length(dirFlat) > 0.01f)
                    {
                        dirFlat = glm::normalize(dirFlat);
                        e.transform.rotation.y = std::atan2(dirFlat.x, dirFlat.z) * (180.0f / kPi);
                    }

                    // Determine preferred combat range per enemy type
                    bool isMeleeOnly = (eType == FPSEnemyType::Rottweiler || eType == FPSEnemyType::IronKnight || eType == FPSEnemyType::Fiend);
                    float stopDist = isMeleeOnly ? 1.45f : 5.5f;

                    if (distToPlayer > stopDist)
                    {
                        glm::vec3 nextPos = e.transform.position + dirFlat * (e.gameplay.moveSpeed * dt);
                        float fl = 0.0f, cl = 4.0f;
                        if (QueryColumnFloorCeil(nextPos.x, nextPos.z, e.transform.position.y - 0.6f, fl, cl))
                        {
                            if (std::abs(fl - (e.transform.position.y - 0.85f)) < 0.75f)
                            {
                                e.transform.position.x = nextPos.x;
                                e.transform.position.z = nextPos.z;
                                float hOff = (eType == FPSEnemyType::Scrag) ? (1.45f + std::sin(m_LevelTime * 4.0f + e.id) * 0.2f) : 0.85f;
                                e.transform.position.y = fl + hOff;
                            }
                        }
                    }

                    // Attack cooldown & type-specific attacks
                    e.gameplay.fireCooldown -= dt;
                    if (e.gameplay.fireCooldown <= 0.0f)
                    {
                        e.gameplay.fireCooldown = e.gameplay.fireRate;
                        glm::vec3 aimAtPlayer = glm::normalize(playerCenter - e.transform.position);

                        if (distToPlayer <= 2.1f)
                        {
                            // Melee cleave (Rottweiler bite, Knight sword, Fiend claws, Ogre chainsaw, Shambler smash)
                            DamagePlayer(e.gameplay.damage, e.transform.position);
                        }
                        else if (!isMeleeOnly && distToPlayer < 24.0f)
                        {
                            // Check clear line of sight before firing
                            float wDist = 0.0f;
                            glm::vec3 wPos, wNorm;
                            if (!RaycastWorldSolid(e.transform.position, aimAtPlayer, distToPlayer, wDist, wPos, wNorm))
                            {
                                if (eType == FPSEnemyType::RottedGrunt)
                                {
                                    // Shotgunner hitscan burst
                                    if (m_Audio) m_Audio->PlaySound3D("shotgun", e.transform.position, 0.70f, 0.9f);
                                    DamagePlayer(8.0f, e.transform.position);
                                }
                                else if (eType == FPSEnemyType::Shambler)
                                {
                                    // Shambler Lightning Arc!
                                    if (m_Audio) m_Audio->PlaySound3D("lightning", e.transform.position, 0.90f, 0.8f);
                                    m_Renderer->AddTransientLine(e.transform.position + glm::vec3(0, 0.5f, 0), playerCenter, glm::vec4(0.4f, 0.95f, 1.0f, 1.0f));
                                    DamagePlayer(18.0f, e.transform.position);
                                }
                                else
                                {
                                    // Spawn Enemy Projectile (Enforcer laser, HellKnight fire spread, Ogre grenade, Scrag acid, Vore homing pod)
                                    Entity& proj = m_Scene->CreateEntity("MonsterProjectile", "EnemyProjectile");
                                    proj.transform.position = e.transform.position + aimAtPlayer * 0.7f + glm::vec3(0.0f, 0.2f, 0.0f);
                                    proj.transform.scale    = glm::vec3(0.26f);
                                    proj.mesh.enabled = true;
                                    proj.mesh.primitive = MeshPrimitive::Sphere;
                                    proj.rigidBody.enabled = true;
                                    proj.rigidBody.velocity = aimAtPlayer * 16.0f;
                                    proj.gameplay.enabled = true;
                                    proj.gameplay.damage = e.gameplay.damage;
                                    proj.gameplay.maxLifetime = 3.5f;
                                    proj.gameplay.subType = 0;

                                    if (eType == FPSEnemyType::ChainsawOgre)
                                    {
                                        proj.gameplay.subType = 1; // Bouncing grenade
                                        proj.rigidBody.velocity = aimAtPlayer * 14.0f + glm::vec3(0.0f, 4.2f, 0.0f);
                                        proj.material.albedoColor = glm::vec4(0.95f, 0.55f, 0.15f, 1.0f);
                                    }
                                    else if (eType == FPSEnemyType::Vore)
                                    {
                                        proj.gameplay.subType = 2; // Homing violet spike ball
                                        proj.material.albedoColor = glm::vec4(0.95f, 0.25f, 1.0f, 1.0f);
                                        proj.material.emissiveColor = glm::vec3(0.90f, 0.20f, 1.0f);
                                        proj.material.emissiveStrength = 4.0f;
                                    }
                                    else if (eType == FPSEnemyType::Scrag)
                                    {
                                        proj.material.albedoColor = glm::vec4(0.35f, 1.0f, 0.20f, 1.0f);
                                        proj.material.emissiveColor = glm::vec3(0.30f, 0.95f, 0.15f);
                                        proj.material.emissiveStrength = 3.5f;
                                    }
                                    else
                                    {
                                        proj.material.albedoColor = glm::vec4(1.0f, 0.35f, 0.12f, 1.0f);
                                        proj.material.emissiveColor = glm::vec3(1.0f, 0.30f, 0.10f);
                                        proj.material.emissiveStrength = 3.8f;
                                    }
                                }
                            }
                        }
                        else if (eType == FPSEnemyType::Fiend && distToPlayer < 9.5f && distToPlayer > 2.5f)
                        {
                            // Fiend lunges forward at high speed!
                            e.transform.position += dirFlat * 1.4f;
                        }
                    }
                }
            }
        }

        m_Scene->FlushDestroyedEntities();
        m_Renderer->UpdateParticles(dt);
    }

    void FPSGameManager::RefreshSaveSlots()
    {
        std::filesystem::create_directories("saves");
        for (int i = 0; i < 6; ++i)
        {
            FPSSaveSlotInfo& info = m_SaveSlots[i];
            info.slotIndex = i;
            std::string path = "saves/save_slot_" + std::to_string(i) + ".json";
            if (!std::filesystem::exists(path))
            {
                info.exists = false;
                info.title = (i == 0) ? "Slot 0 (QuickSave) - Empty" : ("Slot " + std::to_string(i) + " - Empty");
                continue;
            }
            try
            {
                std::ifstream in(path);
                json j = json::parse(in);
                info.exists     = true;
                info.campaign   = j.value("campaign", 1);
                info.levelIndex = j.value("levelIndex", 1);
                info.health     = static_cast<int>(j.value("playerHealth", 100.0f));
                info.armor      = static_cast<int>(j.value("playerArmor", 0.0f));
                info.timestamp  = j.value("timestamp", "");
                std::string lvlTitle = j.value("levelTitle", GetLevelCode(info.campaign, info.levelIndex));
                char buf[256];
                std::snprintf(buf, sizeof(buf), "%s: C%dM%d (%s) | HP:%d AP:%d | %s",
                    (i == 0) ? "QuickSave" : ("Slot " + std::to_string(i)).c_str(),
                    info.campaign, info.levelIndex, lvlTitle.c_str(), info.health, info.armor, info.timestamp.c_str());
                info.title = buf;
            }
            catch (...)
            {
                info.exists = false;
            }
        }
    }

    bool FPSGameManager::SaveGame(int slotIndex, std::string* outMsg)
    {
        if (!m_Scene) return false;
        slotIndex = std::clamp(slotIndex, 0, 5);
        std::filesystem::create_directories("saves");

        try
        {
            std::time_t now = std::time(nullptr);
            char timeBuf[64]{};
            std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

            json j;
            j["campaign"]       = m_Campaign;
            j["levelIndex"]     = m_LevelIndex;
            j["levelCode"]      = m_LevelCode;
            j["levelTitle"]     = m_LevelTitle;
            j["timestamp"]      = timeBuf;
            j["playerFeetPos"]  = { m_PlayerFeetPos.x, m_PlayerFeetPos.y, m_PlayerFeetPos.z };
            j["playerYaw"]      = m_PlayerYaw;
            j["playerPitch"]    = m_PlayerPitch;
            j["playerHealth"]   = m_PlayerHealth;
            j["playerArmor"]    = m_PlayerArmor;
            j["currentWeapon"]  = static_cast<int>(m_CurrentWeapon);
            j["weaponUnlocked"] = m_WeaponUnlocked;
            j["ammo"]           = m_Ammo;
            j["hasRedKey"]      = m_HasRedKey;
            j["hasBlueKey"]     = m_HasBlueKey;
            j["killsInLevel"]   = m_KillsInLevel;
            j["secretsFound"]   = m_SecretsFound;
            j["levelTime"]      = m_LevelTime;
            j["score"]          = m_Scene->score;

            // Save alive enemies & their positions/health
            json enemiesArr = json::array();
            for (const auto& e : m_Scene->GetEntities())
            {
                if (!e.alive || e.tag.tag != "Enemy")
                    continue;
                enemiesArr.push_back({
                    {"type", e.gameplay.subType},
                    {"pos", { e.transform.position.x, e.transform.position.y, e.transform.position.z }},
                    {"yaw", e.transform.rotation.y},
                    {"hp",  e.gameplay.health},
                    {"state", e.gameplay.state}
                });
            }
            j["enemies"] = enemiesArr;

            // Save door states
            json doorsArr = json::array();
            for (const auto& e : m_Scene->GetEntities())
            {
                if (!e.alive || e.tag.tag != "Door")
                    continue;
                doorsArr.push_back({
                    {"basePos", { e.gameplay.basePos.x, e.gameplay.basePos.y, e.gameplay.basePos.z }},
                    {"state", e.gameplay.state}
                });
            }
            j["doors"] = doorsArr;

            std::string path = "saves/save_slot_" + std::to_string(slotIndex) + ".json";
            std::ofstream out(path, std::ios::out | std::ios::trunc);
            out << j.dump(2);
            out.close();

            RefreshSaveSlots();
            if (outMsg) *outMsg = "Saved game to " + path;
            Log::Gameplay("Saved FPS game state to " + path);
            return true;
        }
        catch (const std::exception& ex)
        {
            if (outMsg) *outMsg = std::string("Save failed: ") + ex.what();
            return false;
        }
    }

    bool FPSGameManager::LoadGame(int slotIndex, std::string* outMsg)
    {
        slotIndex = std::clamp(slotIndex, 0, 5);
        std::string path = "saves/save_slot_" + std::to_string(slotIndex) + ".json";
        if (!std::filesystem::exists(path))
        {
            if (outMsg) *outMsg = "Save slot does not exist.";
            return false;
        }

        try
        {
            std::ifstream in(path);
            json j = json::parse(in);

            int camp = j.value("campaign", 1);
            int lvl  = j.value("levelIndex", 1);
            if (!LoadCampaignLevel(camp, lvl, false))
                return false;

            if (j.contains("playerFeetPos") && j["playerFeetPos"].is_array())
            {
                m_PlayerFeetPos = glm::vec3(
                    j["playerFeetPos"][0].get<float>(),
                    j["playerFeetPos"][1].get<float>(),
                    j["playerFeetPos"][2].get<float>());
            }
            m_PlayerYaw      = j.value("playerYaw", 0.0f);
            m_PlayerPitch    = j.value("playerPitch", 0.0f);
            m_PlayerHealth   = j.value("playerHealth", 100.0f);
            m_PlayerArmor    = j.value("playerArmor", 50.0f);
            m_HasRedKey      = j.value("hasRedKey", false);
            m_HasBlueKey     = j.value("hasBlueKey", false);
            m_KillsInLevel   = j.value("killsInLevel", 0);
            m_SecretsFound   = j.value("secretsFound", 0);
            m_LevelTime      = j.value("levelTime", 0.0f);
            m_Scene->score   = j.value("score", 0);

            if (j.contains("weaponUnlocked") && j["weaponUnlocked"].is_array())
            {
                for (size_t k = 0; k < 5 && k < j["weaponUnlocked"].size(); ++k)
                    m_WeaponUnlocked[k] = j["weaponUnlocked"][k].get<bool>();
            }
            if (j.contains("ammo") && j["ammo"].is_array())
            {
                for (size_t k = 0; k < 5 && k < j["ammo"].size(); ++k)
                    m_Ammo[k] = j["ammo"][k].get<int>();
            }
            SelectWeapon(j.value("currentWeapon", 1));

            // Replace spawned enemies with saved alive enemies
            if (j.contains("enemies") && j["enemies"].is_array())
            {
                for (auto& e : m_Scene->GetEntities())
                {
                    if (e.tag.tag == "Enemy")
                        e.alive = false;
                }
                m_Scene->FlushDestroyedEntities();
                int savedTotal = m_TotalMonstersInLevel;
                m_TotalMonstersInLevel = 0;

                for (const auto& ej : j["enemies"])
                {
                    FPSEnemyType t = static_cast<FPSEnemyType>(ej.value("type", 0));
                    glm::vec3 p(ej["pos"][0].get<float>(), ej["pos"][1].get<float>(), ej["pos"][2].get<float>());
                    SpawnEnemyByType(t, p - glm::vec3(0.0f, 0.85f, 0.0f), ej.value("yaw", 0.0f));
                    Entity& spawned = m_Scene->GetEntities().back();
                    spawned.transform.position = p;
                    spawned.gameplay.health = ej.value("hp", spawned.gameplay.maxHealth);
                    spawned.gameplay.state  = ej.value("state", 0);
                }
                m_TotalMonstersInLevel = std::max(savedTotal, m_TotalMonstersInLevel);
            }

            // Restore door states
            if (j.contains("doors") && j["doors"].is_array())
            {
                for (const auto& dj : j["doors"])
                {
                    glm::vec3 bp(dj["basePos"][0].get<float>(), dj["basePos"][1].get<float>(), dj["basePos"][2].get<float>());
                    int st = dj.value("state", 0);
                    for (auto& e : m_Scene->GetEntities())
                    {
                        if (e.alive && e.tag.tag == "Door" && glm::length(e.gameplay.basePos - bp) < 0.5f)
                        {
                            e.gameplay.state = st;
                            if (st == 2)
                                e.transform.position = e.gameplay.targetPos;
                        }
                    }
                }
            }

            if (outMsg) *outMsg = "Loaded game from " + path;
            Log::Gameplay("Loaded FPS game state from " + path);
            return true;
        }
        catch (const std::exception& ex)
        {
            if (outMsg) *outMsg = std::string("Load failed: ") + ex.what();
            return false;
        }
    }

    void FPSGameManager::RenderUI(const ImVec2& vpMin, const ImVec2& vpSize, ImDrawList* dl)
    {
        if (!dl || vpSize.x < 100.0f || vpSize.y < 100.0f)
            return;

        // 1. Damage Blood Vignette & Pickup Gold Flash Overlays
        if (m_DamageFlashAlpha > 0.01f)
        {
            ImU32 col = IM_COL32(185, 12, 10, static_cast<int>(m_DamageFlashAlpha * 165.0f));
            dl->AddRectFilled(vpMin, ImVec2(vpMin.x + vpSize.x, vpMin.y + vpSize.y), col);
        }
        if (m_PickupFlashAlpha > 0.01f)
        {
            ImU32 col = IM_COL32(235, 195, 55, static_cast<int>(m_PickupFlashAlpha * 110.0f));
            dl->AddRectFilled(vpMin, ImVec2(vpMin.x + vpSize.x, vpMin.y + vpSize.y), col);
        }

        // 2. Center Crosshair & Muzzle Flash Reticle
        ImVec2 center(vpMin.x + vpSize.x * 0.5f, vpMin.y + vpSize.y * 0.5f);
        if (!IsInMenu())
        {
            ImU32 chCol = IM_COL32(255, 215, 95, 225);
            dl->AddLine(ImVec2(center.x - 10, center.y), ImVec2(center.x - 3, center.y), chCol, 2.0f);
            dl->AddLine(ImVec2(center.x + 3, center.y),  ImVec2(center.x + 10, center.y), chCol, 2.0f);
            dl->AddLine(ImVec2(center.x, center.y - 10), ImVec2(center.x, center.y - 3), chCol, 2.0f);
            dl->AddLine(ImVec2(center.x, center.y + 3),  ImVec2(center.x, center.y + 10), chCol, 2.0f);
        }

        // 3. Quake-1 Style Status Bar HUD (Bottom Bar + Top Level Banner)
        {
            char buf[256];

            // Top-Left Level & Campaign Banner
            ImVec2 topBarMin(vpMin.x + 14.0f, vpMin.y + 14.0f);
            dl->AddRectFilled(topBarMin, ImVec2(topBarMin.x + 375.0f, topBarMin.y + 62.0f), IM_COL32(14, 11, 9, 215), 5.0f);
            dl->AddRect(topBarMin, ImVec2(topBarMin.x + 375.0f, topBarMin.y + 62.0f), IM_COL32(145, 105, 55, 200), 5.0f, 0, 1.5f);

            std::snprintf(buf, sizeof(buf), "CAMPAIGN %d - MAP %d/5 [%s]: %s",
                m_Campaign, m_LevelIndex, m_LevelCode.c_str(), m_LevelTitle.c_str());
            dl->AddText(ImVec2(topBarMin.x + 10.0f, topBarMin.y + 8.0f), IM_COL32(245, 205, 115, 255), buf);

            std::snprintf(buf, sizeof(buf), "KILLS: %d / %d   SECRETS: %d / %d   STYLE: %s",
                m_KillsInLevel, m_TotalMonstersInLevel, m_SecretsFound, m_TotalSecrets, m_LevelStyle.c_str());
            dl->AddText(ImVec2(topBarMin.x + 10.0f, topBarMin.y + 32.0f), IM_COL32(205, 185, 155, 255), buf);

            // Top-Right Quick Hotkeys Hint
            ImVec2 topRight(vpMin.x + vpSize.x - 295.0f, vpMin.y + 14.0f);
            dl->AddRectFilled(topRight, ImVec2(topRight.x + 281.0f, topRight.y + 48.0f), IM_COL32(14, 11, 9, 210), 5.0f);
            dl->AddRect(topRight, ImVec2(topRight.x + 281.0f, topRight.y + 48.0f), IM_COL32(145, 105, 55, 180), 5.0f);
            dl->AddText(ImVec2(topRight.x + 10.0f, topRight.y + 7.0f),  IM_COL32(235, 195, 120, 255), "[ESC] Game Menu & Settings");
            dl->AddText(ImVec2(topRight.x + 10.0f, topRight.y + 25.0f), IM_COL32(185, 215, 175, 255), "[F5] QuickSave   [F9] QuickLoad");

            // Center Status Notification Banner
            if (m_StatusMessageTimer > 0.0f && !m_StatusMessage.empty())
            {
                ImVec2 tSz = ImGui::CalcTextSize(m_StatusMessage.c_str());
                ImVec2 bMin(center.x - tSz.x * 0.5f - 18.0f, vpMin.y + 88.0f);
                ImVec2 bMax(center.x + tSz.x * 0.5f + 18.0f, vpMin.y + 120.0f);
                dl->AddRectFilled(bMin, bMax, IM_COL32(28, 18, 12, 230), 5.0f);
                dl->AddRect(bMin, bMax, IM_COL32(235, 175, 65, 235), 5.0f, 0, 2.0f);
                dl->AddText(ImVec2(bMin.x + 18.0f, bMin.y + 8.0f), IM_COL32(255, 225, 130, 255), m_StatusMessage.c_str());
            }

            // Bottom Quake-1 Status Bar (Armor | Health | Current Weapon & Ammo | Weapon Rack 1..5 | Keys)
            float barW = std::min(vpSize.x - 28.0f, 820.0f);
            float barH = 66.0f;
            ImVec2 barMin(center.x - barW * 0.5f, vpMin.y + vpSize.y - barH - 12.0f);
            ImVec2 barMax(barMin.x + barW, barMin.y + barH);

            dl->AddRectFilled(barMin, barMax, IM_COL32(22, 17, 13, 235), 6.0f);
            dl->AddRect(barMin, barMax, IM_COL32(155, 110, 58, 235), 6.0f, 0, 2.0f);

            // Armor Box
            std::snprintf(buf, sizeof(buf), "ARMOR\n  %03d", static_cast<int>(m_PlayerArmor));
            dl->AddText(ImVec2(barMin.x + 18.0f, barMin.y + 14.0f), IM_COL32(235, 195, 75, 255), buf);

            // Health Box
            ImU32 hpCol = (m_PlayerHealth > 35.0f) ? IM_COL32(235, 85, 75, 255) : IM_COL32(255, 40, 40, 255);
            std::snprintf(buf, sizeof(buf), "HEALTH\n  %03d", static_cast<int>(m_PlayerHealth));
            dl->AddText(ImVec2(barMin.x + 115.0f, barMin.y + 14.0f), hpCol, buf);

            // Ammo & Active Weapon Box
            int wIdx = static_cast<int>(m_CurrentWeapon);
            if (wIdx == 0)
                std::snprintf(buf, sizeof(buf), "%s\nAMMO: INFINITE", GetWeaponName(m_CurrentWeapon));
            else
                std::snprintf(buf, sizeof(buf), "%s\nAMMO: %d / %d", GetWeaponName(m_CurrentWeapon), m_Ammo[wIdx], m_MaxAmmo[wIdx]);
            dl->AddText(ImVec2(barMin.x + 215.0f, barMin.y + 14.0f), IM_COL32(245, 225, 175, 255), buf);

            // Weapon Slots 1..5
            const char* wShort[5] = { "[1]AXE", "[2]SG", "[3]SNG", "[4]RL", "[5]LG" };
            for (int w = 0; w < 5; ++w)
            {
                ImVec2 wPos(barMin.x + 475.0f + w * 50.0f, barMin.y + 14.0f);
                ImU32 col = (w == wIdx)
                    ? IM_COL32(255, 230, 80, 255)
                    : (m_WeaponUnlocked[w] ? IM_COL32(180, 165, 140, 235) : IM_COL32(85, 75, 65, 160));
                dl->AddText(wPos, col, wShort[w]);
                if (w > 0)
                {
                    std::snprintf(buf, sizeof(buf), "%d", m_Ammo[w]);
                    dl->AddText(ImVec2(wPos.x + 8.0f, wPos.y + 20.0f), col, buf);
                }
            }

            // Keys Held Box
            dl->AddText(ImVec2(barMin.x + 730.0f, barMin.y + 10.0f),
                m_HasRedKey ? IM_COL32(255, 65, 55, 255) : IM_COL32(75, 55, 55, 160), "RED KEY");
            dl->AddText(ImVec2(barMin.x + 730.0f, barMin.y + 34.0f),
                m_HasBlueKey ? IM_COL32(75, 155, 255, 255) : IM_COL32(55, 65, 85, 160), "BLUE KEY");
        }

        // 4. Full Interactive ImGui Menus (Main Menu, Campaign Select, Pause, Graphics & Sound Settings, Save & Load)
        if (!IsInMenu())
            return;

        ImVec2 menuSize(620.0f, 520.0f);
        ImGui::SetNextWindowPos(ImVec2(vpMin.x + (vpSize.x - menuSize.x) * 0.5f, vpMin.y + (vpSize.y - menuSize.y) * 0.5f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(menuSize, ImGuiCond_Always);

        ImGuiWindowFlags mFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.09f, 0.07f, 0.06f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.68f, 0.48f, 0.24f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.22f, 0.14f, 0.08f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.32f, 0.20f, 0.10f, 1.0f));

        if (ImGui::Begin("ELDER SLIPGATE: QUAKE-STYLE SINGLE-PLAYER FPS (Powered by Hargon Engine & BadLevel)", nullptr, mFlags))
        {
            if (m_MenuState == FPSMenuState::MainMenu || m_MenuState == FPSMenuState::PauseMenu)
            {
                bool isPause = (m_MenuState == FPSMenuState::PauseMenu);
                ImGui::TextColored(ImVec4(0.98f, 0.78f, 0.35f, 1.0f), isPause ? "=== GAME PAUSED ===" : "=== ELDER SLIPGATE: MAIN MENU ===");
                ImGui::TextDisabled("2 Campaigns (10 Levels) | 5 Weapons | 10 Enemy Types | Quake-1 Blood & Lighting");
                ImGui::Separator();
                ImGui::Spacing();

                if (isPause)
                {
                    if (ImGui::Button("RESUME GAME [ESC]", ImVec2(-1.0f, 38.0f)))
                        SetMenuState(FPSMenuState::None);
                }
                else
                {
                    std::string contLabel = "CONTINUE CURRENT MISSION (" + m_LevelCode + ": " + m_LevelTitle + ")";
                    if (ImGui::Button(contLabel.c_str(), ImVec2(-1.0f, 38.0f)))
                        SetMenuState(FPSMenuState::None);
                }

                if (ImGui::Button("NEW GAME / SELECT CAMPAIGN & LEVEL (2 Campaigns x 5 Levels)", ImVec2(-1.0f, 38.0f)))
                {
                    m_PreviousMenuState = m_MenuState;
                    SetMenuState(FPSMenuState::CampaignSelect);
                }

                if (ImGui::Button("SAVE GAME", ImVec2(-1.0f, 36.0f)))
                {
                    m_PreviousMenuState = m_MenuState;
                    m_SaveMenuIsSaving = true;
                    RefreshSaveSlots();
                    SetMenuState(FPSMenuState::SaveLoadMenu);
                }

                if (ImGui::Button("LOAD GAME", ImVec2(-1.0f, 36.0f)))
                {
                    m_PreviousMenuState = m_MenuState;
                    m_SaveMenuIsSaving = false;
                    RefreshSaveSlots();
                    SetMenuState(FPSMenuState::SaveLoadMenu);
                }

                if (ImGui::Button("GRAPHICS & SOUND SETTINGS", ImVec2(-1.0f, 36.0f)))
                {
                    m_PreviousMenuState = m_MenuState;
                    SetMenuState(FPSMenuState::SettingsMenu);
                }

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.58f, 1.0f), "WEAPONS ARSENAL (Keys 1-5 or Scroll Wheel):");
                ImGui::BulletText("1: Runic Battle-Axe  |  2: Buckshot Shotgun  |  3: Super Perforator Nailgun");
                ImGui::BulletText("4: Ogre Rocket Launcher (Splash + Gibs)  |  5: Thunderbolt Lightning Shaft");
                ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.58f, 1.0f), "CONTROLS:");
                ImGui::BulletText("[WASD] Move/Strafe   [Mouse / Arrows] Look   [LMB / F] Fire   [SPACE] Jump");
                ImGui::BulletText("[F5] QuickSave   [F9] QuickLoad   [ESC] Pause / Menu");

                ImGui::Spacing();
                if (ImGui::Button("QUIT TO DESKTOP", ImVec2(-1.0f, 34.0f)))
                {
                    if (m_Window) m_Window->RequestClose();
                }
            }
            else if (m_MenuState == FPSMenuState::CampaignSelect)
            {
                ImGui::TextColored(ImVec4(0.98f, 0.78f, 0.35f, 1.0f), "=== SELECT CAMPAIGN & MISSION (10 BadLevel Procedural Maps) ===");
                ImGui::Separator();

                if (ImGui::Button("CAMPAIGN I: THE ELDER WARD (5 Levels)", ImVec2(290.0f, 34.0f)))
                    m_SelectedCampaignTab = 1;
                ImGui::SameLine();
                if (ImGui::Button("CAMPAIGN II: THE IRON CITADEL (5 Levels)", ImVec2(290.0f, 34.0f)))
                    m_SelectedCampaignTab = 2;

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.90f, 0.82f, 0.60f, 1.0f), "%s", GetCampaignTitle(m_SelectedCampaignTab));
                ImGui::Separator();

                static const char* kC1Names[5] = {
                    "C1M1: The Slipgate Sanctum (Gothic Vaults, 1 Storey)",
                    "C1M2: Vault of the Iron Keys (Castle Keep, 2 Storeys)",
                    "C1M3: Catacombs of Sorrow (Gothic Fortress, 2 Storeys, 2 Keys)",
                    "C1M4: The Sunken Basilica (Egyptian Hypostyle, 3 Storeys)",
                    "C1M5: Altar of the Elder God (Grand Gothic Citadel Boss Finale)"
                };
                static const char* kC2Names[5] = {
                    "C2M1: Perimeter Breach (Underground Bunker, 2 Storeys)",
                    "C2M2: Slag Processing Works (Industrial Catwalks & Lifts, 2 Storeys)",
                    "C2M3: Logistics Compound (Open Sky Courtyard & Buildings)",
                    "C2M4: Reactor Sub-Level 9 (Sci-Fi Slipgate Complex, 3 Storeys)",
                    "C2M5: The Iron Citadel Core (Mega-Hangar & Shambler Finale)"
                };

                for (int m = 1; m <= 5; ++m)
                {
                    const char* label = (m_SelectedCampaignTab == 1) ? kC1Names[m - 1] : kC2Names[m - 1];
                    if (ImGui::Button(label, ImVec2(-1.0f, 36.0f)))
                    {
                        LoadCampaignLevel(m_SelectedCampaignTab, m, m == 1);
                        SetMenuState(FPSMenuState::None);
                    }
                }

                ImGui::Separator();
                static int s_CustomSeed = 777;
                ImGui::SetNextItemWidth(160.0f);
                ImGui::InputInt("Custom BadLevel Seed", &s_CustomSeed);
                ImGui::SameLine();
                if (ImGui::Button("Generate Fresh BadLevel Map Now", ImVec2(-1.0f, 28.0f)))
                {
                    GenerateAndLoadLevel(m_SelectedCampaignTab, m_LevelIndex, s_CustomSeed);
                    SetMenuState(FPSMenuState::None);
                }

                ImGui::Spacing();
                if (ImGui::Button("<< BACK", ImVec2(-1.0f, 34.0f)))
                    SetMenuState(m_PreviousMenuState);
            }
            else if (m_MenuState == FPSMenuState::SettingsMenu)
            {
                ImGui::TextColored(ImVec4(0.98f, 0.78f, 0.35f, 1.0f), "=== GRAPHICS, AUDIO & CONTROLS SETTINGS ===");
                ImGui::Separator();

                ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "GRAPHICS & WORLD ENVIRONMENT (NVRHI + Godot ToneMapper):");
                ImGui::SliderFloat("Field of View (FOV)", &m_Settings.fovDegrees, 60.0f, 110.0f, "%.0f deg");
                ImGui::SliderFloat("Brightness / Gamma", &m_Settings.brightness, 0.5f, 2.0f, "%.2f");
                ImGui::SliderFloat("Atmospheric Fog Density", &m_Settings.fogMultiplier, 0.0f, 2.5f, "%.2f");
                ImGui::SliderFloat("Quake Blood & Gore Intensity", &m_Settings.bloodGoreScale, 0.25f, 2.5f, "%.2fx");
                ImGui::Checkbox("Dynamic Torch & Muzzle Point Lights", &m_Settings.dynamicLights);
                if (m_Scene)
                {
                    const char* tmModes[] = { "Linear", "Reinhard", "Filmic (Hable)", "ACES Fitted", "AgX (Godot 4.3+)" };
                    int curTm = static_cast<int>(m_Scene->worldEnv.tonemapMode);
                    if (ImGui::Combo("HDR ToneMapper", &curTm, tmModes, 5))
                        m_Scene->worldEnv.tonemapMode = static_cast<ToneMapperMode>(curTm);
                    ImGui::SliderFloat("Tonemap Exposure", &m_Scene->worldEnv.tonemapExposure, 0.4f, 2.2f, "%.2f");
                    ImGui::SliderFloat("Color Contrast",   &m_Scene->worldEnv.adjustmentContrast, 0.7f, 1.5f, "%.2f");
                    ImGui::SliderFloat("Color Saturation", &m_Scene->worldEnv.adjustmentSaturation, 0.0f, 2.0f, "%.2f");
                    ImGui::Checkbox("Fast Frustum Culling", &m_Scene->worldEnv.frustumCullingEnabled);
                }

                const char* resOptions[] = { "1280 x 720 (HD)", "1600 x 900 (HD+)", "1920 x 1080 (Full HD)" };
                if (ImGui::Combo("Window Resolution", &m_Settings.resolutionIndex, resOptions, 3))
                {
                    if (m_Window && m_Window->GetNativeWindow())
                    {
                        int w = (m_Settings.resolutionIndex == 0) ? 1280 : ((m_Settings.resolutionIndex == 1) ? 1600 : 1920);
                        int h = (m_Settings.resolutionIndex == 0) ? 720  : ((m_Settings.resolutionIndex == 1) ? 900  : 1080);
                        glfwSetWindowSize(m_Window->GetNativeWindow(), w, h);
                    }
                }

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "SOUND SETTINGS (miniaudio):");
                ImGui::SliderFloat("Master Volume", &m_Settings.masterVolume, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("SFX Volume",    &m_Settings.sfxVolume,    0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Ambient Volume",&m_Settings.ambientVolume,0.0f, 1.0f, "%.2f");
                ImGui::Checkbox("Mute All Audio",   &m_Settings.muted);

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "MOUSE & INPUT:");
                ImGui::SliderFloat("Mouse Look Sensitivity", &m_Settings.mouseSensitivity, 0.05f, 0.60f, "%.2f");

                ApplySettings();

                ImGui::Spacing();
                if (ImGui::Button("SAVE SETTINGS & RETURN", ImVec2(-1.0f, 36.0f)))
                {
                    SaveSettings("settings.json");
                    SetMenuState(m_PreviousMenuState);
                }
            }
            else if (m_MenuState == FPSMenuState::SaveLoadMenu)
            {
                ImGui::TextColored(ImVec4(0.98f, 0.78f, 0.35f, 1.0f),
                    m_SaveMenuIsSaving ? "=== SAVE GAME TO SLOT ===" : "=== LOAD SAVED GAME ===");
                ImGui::Separator();

                for (int i = 0; i < 6; ++i)
                {
                    const auto& slot = m_SaveSlots[i];
                    std::string btnLabel = (m_SaveMenuIsSaving ? "[SAVE] " : "[LOAD] ") + slot.title + "###slot_" + std::to_string(i);
                    if (ImGui::Button(btnLabel.c_str(), ImVec2(-1.0f, 38.0f)))
                    {
                        std::string msg;
                        if (m_SaveMenuIsSaving)
                        {
                            if (SaveGame(i, &msg))
                            {
                                m_StatusMessage = msg;
                                m_StatusMessageTimer = 3.5f;
                                SetMenuState(FPSMenuState::None);
                            }
                        }
                        else if (slot.exists)
                        {
                            if (LoadGame(i, &msg))
                            {
                                m_StatusMessage = msg;
                                m_StatusMessageTimer = 3.5f;
                                SetMenuState(FPSMenuState::None);
                            }
                        }
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("<< BACK", ImVec2(-1.0f, 34.0f)))
                    SetMenuState(m_PreviousMenuState);
            }
            else if (m_MenuState == FPSMenuState::LevelComplete || m_MenuState == FPSMenuState::CampaignVictory)
            {
                bool campWon = (m_MenuState == FPSMenuState::CampaignVictory);
                ImGui::TextColored(ImVec4(0.45f, 1.0f, 0.55f, 1.0f),
                    campWon ? "=== CAMPAIGN COMPLETED! THE ELDER SLIPGATE IS SEALED! ===" : "=== LEVEL COMPLETED! ENTERING SLIPGATE... ===");
                ImGui::Separator();
                ImGui::Text("Completed Map : %s (%s)", m_LevelCode.c_str(), m_LevelTitle.c_str());
                ImGui::Text("Monsters Slain: %d / %d", m_KillsInLevel, m_TotalMonstersInLevel);
                ImGui::Text("Secrets Found : %d / %d", m_SecretsFound, m_TotalSecrets);
                ImGui::Text("Level Time    : %.1f seconds", m_LevelTime);
                ImGui::Separator();

                if (!campWon)
                {
                    if (ImGui::Button("PROCEED TO NEXT LEVEL >>", ImVec2(-1.0f, 42.0f)))
                    {
                        LoadCampaignLevel(m_Campaign, m_LevelIndex + 1, false);
                        SetMenuState(FPSMenuState::None);
                    }
                }
                else if (m_Campaign == 1)
                {
                    if (ImGui::Button("START CAMPAIGN II: THE IRON CITADEL (C2M1) >>", ImVec2(-1.0f, 42.0f)))
                    {
                        LoadCampaignLevel(2, 1, false);
                        SetMenuState(FPSMenuState::None);
                    }
                }

                if (ImGui::Button("RETURN TO MAIN MENU", ImVec2(-1.0f, 36.0f)))
                    SetMenuState(FPSMenuState::MainMenu);
            }
            else if (m_MenuState == FPSMenuState::GameOver)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "=== YOU WERE SLAIN IN THE SLIPGATE DEPTHS ===");
                ImGui::Separator();
                if (ImGui::Button("RESTART CURRENT LEVEL", ImVec2(-1.0f, 40.0f)))
                {
                    LoadCampaignLevel(m_Campaign, m_LevelIndex, true);
                    SetMenuState(FPSMenuState::None);
                }
                if (ImGui::Button("LOAD SAVED GAME", ImVec2(-1.0f, 36.0f)))
                {
                    m_PreviousMenuState = FPSMenuState::GameOver;
                    m_SaveMenuIsSaving = false;
                    RefreshSaveSlots();
                    SetMenuState(FPSMenuState::SaveLoadMenu);
                }
                if (ImGui::Button("MAIN MENU", ImVec2(-1.0f, 36.0f)))
                    SetMenuState(FPSMenuState::MainMenu);
            }
        }
        ImGui::End();
        ImGui::PopStyleColor(4);
    }
}
