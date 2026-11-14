#pragma once
#include "Scene.h"
#include <string>

namespace Hargon
{
    struct GameProjectConfig
    {
        std::string gameTitle = "Starforge Outpost: Crystal Defense";
        std::string startupScene = "assets/scenes/starforge_outpost.hscene";
        uint32_t windowWidth = 1440;
        uint32_t windowHeight = 850;
        std::string preferredBackend = "Vulkan";
        int maxWaves = 3;
    };

    class SceneSerializer
    {
    public:
        static std::string SerializeToString(const Scene& scene);
        static bool DeserializeFromString(Scene& scene, const std::string& jsonText);

        static bool SaveToFile(const Scene& scene, const std::string& filePath);
        static bool LoadFromFile(Scene& scene, const std::string& filePath);

        static bool SaveProjectConfig(const GameProjectConfig& config, const std::string& filePath);
        static bool LoadProjectConfig(GameProjectConfig& config, const std::string& filePath);
    };
}
