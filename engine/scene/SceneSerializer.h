#pragma once
#include "Scene.h"
#include <string>
#include <vector>

namespace Hargon
{
    struct GameProjectConfig
    {
        std::string projectName = "StarforgeOutpost";
        std::string gameTitle = "Starforge Outpost: Crystal Defense";
        std::string genre = "Sci-Fi Action / Defense";
        std::string author = "Hargon Studio";
        std::string version = "1.0.0";
        std::string projectRootDir = "projects/StarforgeOutpost";
        std::string startupScene = "projects/StarforgeOutpost/scenes/starforge_outpost.hscene";
        std::string executableName = "StarforgeOutpost.exe";
        std::string gameplayModule = "SceneAndLua"; // "SceneAndLua", "ElderSlipgateFPS", etc.
        bool enableBadLevel = false;                // Optional BadLevel procedural level generator integration
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

        // Godot PackedScene-inspired Entity Prefab serialization (.hprefab)
        static bool SaveEntityPrefab(const Entity& entity, const std::string& filePath);
        static EntityID InstantiatePrefab(Scene& scene, const std::string& filePath, const glm::vec3& positionOffset = glm::vec3(0.0f));
    };
}
