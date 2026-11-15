#pragma once
#include "../scene/Scene.h"
#include <string>

namespace Hargon
{
    // Optional, strictly read-only integration with BadLevel procedural JSON output.
    // Never modifies external/BadLevel; imports generated levels into any Scene as reusable 3D entities.
    class BadLevelImporter
    {
    public:
        static bool ImportJsonToScene(
            const std::string& jsonPath,
            Scene& scene,
            float quakeToMeters = 0.045f,
            int maxBrushes = 450,
            std::string* outStatus = nullptr);

        static bool GenerateMapJson(
            const std::string& outJsonPath,
            int seed = 1337,
            const std::string& recipe = "classic",
            const std::string& style = "gothic",
            std::string* outStatus = nullptr);
    };
}
