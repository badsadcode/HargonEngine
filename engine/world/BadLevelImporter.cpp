#include "BadLevelImporter.h"
#include "../core/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>

using json = nlohmann::json;

namespace Hargon
{
    bool BadLevelImporter::ImportJsonToScene(
        const std::string& jsonPath,
        Scene& scene,
        float quakeToMeters,
        int maxBrushes,
        std::string* outStatus)
    {
        try
        {
            std::ifstream in(jsonPath);
            if (!in)
            {
                if (outStatus) *outStatus = "Could not open BadLevel JSON: " + jsonPath;
                return false;
            }

            json root;
            in >> root;

            if (!root.contains("brushes") || !root["brushes"].is_array())
            {
                if (outStatus) *outStatus = "Invalid BadLevel JSON (missing brushes array): " + jsonPath;
                return false;
            }

            // Compute world centering offset from first player_start or average brush center
            glm::vec3 centerOffset(0.0f);
            bool foundStart = false;
            glm::vec3 playerStartWorld(0.0f, 1.2f, 0.0f);
            if (root.contains("entities") && root["entities"].is_array())
            {
                for (const auto& ej : root["entities"])
                {
                    if (ej.value("kind", "") == "player_start" && ej.contains("pos") && ej["pos"].size() >= 3)
                    {
                        float qx = ej["pos"][0].get<float>();
                        float qy = ej["pos"][1].get<float>();
                        float qz = ej["pos"][2].get<float>();
                        centerOffset = glm::vec3(qx * quakeToMeters, qz * quakeToMeters, qy * quakeToMeters);
                        playerStartWorld = glm::vec3(0.0f, 1.1f, 0.0f);
                        foundStart = true;
                        break;
                    }
                }
            }
            (void)foundStart;

            int importedBrushes = 0;
            for (const auto& b : root["brushes"])
            {
                if (importedBrushes >= maxBrushes)
                    break;
                if (!b.contains("min") || !b.contains("max") || b["min"].size() < 3 || b["max"].size() < 3)
                    continue;

                float minX = b["min"][0].get<float>() * quakeToMeters - centerOffset.x;
                float minY = b["min"][2].get<float>() * quakeToMeters - centerOffset.y;
                float minZ = b["min"][1].get<float>() * quakeToMeters - centerOffset.z;
                float maxX = b["max"][0].get<float>() * quakeToMeters - centerOffset.x;
                float maxY = b["max"][2].get<float>() * quakeToMeters - centerOffset.y;
                float maxZ = b["max"][1].get<float>() * quakeToMeters - centerOffset.z;

                glm::vec3 sz = glm::max(glm::vec3(maxX - minX, maxY - minY, maxZ - minZ), glm::vec3(0.15f));
                glm::vec3 ctr(0.5f * (minX + maxX), 0.5f * (minY + maxY), 0.5f * (minZ + maxZ));

                std::string topRole = b.value("top", "wall");
                Entity& blk = scene.CreateEntity("BadLevelBrush_" + std::to_string(importedBrushes + 1), "Environment");
                blk.transform.position = ctr;
                blk.transform.scale = sz;
                blk.mesh.enabled = true;
                blk.mesh.primitive = MeshPrimitive::Cube;
                blk.material.patternType = (topRole == "floor") ? 5 : 2;
                blk.material.uvScale = 1.5f;
                blk.material.albedoColor = (topRole == "floor")
                    ? glm::vec4(0.32f, 0.30f, 0.28f, 1.0f)
                    : glm::vec4(0.45f, 0.42f, 0.38f, 1.0f);
                blk.material.roughness = 0.72f;
                blk.collider.enabled = true;
                blk.collider.isSphere = false;
                blk.collider.boxHalfExtents = sz * 0.5f;
                blk.rigidBody.enabled = true;
                blk.rigidBody.isKinematic = true;

                ++importedBrushes;
            }

            // Place player at origin start if present
            if (auto players = scene.FindEntitiesByTag("Player"); !players.empty())
            {
                if (Entity* p = scene.GetEntity(players[0]))
                    p->transform.position = playerStartWorld + glm::vec3(0.0f, 0.5f, 0.0f);
            }

            std::string msg = "Imported " + std::to_string(importedBrushes) + " BadLevel brushes from '" + jsonPath + "'.";
            Log::Info(msg);
            if (outStatus) *outStatus = msg;
            return true;
        }
        catch (const std::exception& ex)
        {
            if (outStatus) *outStatus = std::string("BadLevel import error: ") + ex.what();
            Log::Error(std::string("BadLevel import error: ") + ex.what());
            return false;
        }
    }

    bool BadLevelImporter::GenerateMapJson(
        const std::string& outJsonPath,
        int seed,
        const std::string& recipe,
        const std::string& style,
        std::string* outStatus)
    {
        if (!std::filesystem::exists("external/BadLevel"))
        {
            if (outStatus) *outStatus = "external/BadLevel repository not present.";
            return false;
        }

        std::filesystem::path outP(outJsonPath);
        if (outP.has_parent_path())
            std::filesystem::create_directories(outP.parent_path());

        std::string pyCmd =
            "python -c \"import sys, json; sys.path.insert(0, 'external/BadLevel'); "
            "from badlevel.recipes import get_recipe, compile_recipe; "
            "from badlevel.generator import generate; from badlevel.export.ir_json import to_dict; "
            "cfg = compile_recipe(get_recipe('" + recipe + "'), " + std::to_string(seed) + "); "
            "cfg.arch_style = '" + style + "'; lvl = generate(cfg); "
            "open('" + outP.generic_string() + "', 'w').write(json.dumps(to_dict(lvl, include_spans=True)))\"";

        int rc = std::system(pyCmd.c_str());
        if (rc == 0 && std::filesystem::exists(outP))
        {
            if (outStatus) *outStatus = "Generated BadLevel map JSON at " + outP.generic_string();
            return true;
        }
        if (outStatus) *outStatus = "BadLevel Python generation exited with code " + std::to_string(rc);
        return false;
    }
}
