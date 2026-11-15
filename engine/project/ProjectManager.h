#pragma once
#include "../scene/SceneSerializer.h"
#include <vector>
#include <string>

namespace Hargon
{
    class ProjectManager
    {
    public:
        // Ensures built-in demonstration projects (ElderSlipgate, AetherRealms3D, StarforgeOutpost) exist on disk
        static void EnsureBuiltInProjects();

        // Discovers all .hproject configurations inside the projects/ directory
        static std::vector<GameProjectConfig> DiscoverProjects(const std::string& rootDir = "projects");

        // Creates a brand-new game project directory, startup .hscene, and project.hproject file
        // templateType: 0 = Empty 3D Scene, 1 = 3D Floating-Island Action-Platformer, 2 = Sci-Fi Rover Defense, 3 = BadLevel FPS Dungeon
        static bool CreateNewProject(
            const std::string& projectSlug,
            const std::string& gameTitle,
            const std::string& genre,
            int templateType,
            bool enableBadLevel,
            GameProjectConfig& outConfig,
            std::string* outStatus = nullptr);
    };
}
