#pragma once
#include "../scene/SceneSerializer.h"
#include <vector>
#include <string>

namespace Hargon
{
    class ProjectManager
    {
    public:
        // Resolves CWD to the standalone game directory or the Hargon workspace root (even when launched from build/Release/)
        static void InitializeWorkingDirectory(const char* argv0 = nullptr);

        // Ensures built-in demonstration projects (ElderSlipgate, AetherRealms3D, StarforgeOutpost) exist on disk
        static void EnsureBuiltInProjects();

        // Discovers all .hproject configurations inside the projects/ directory
        static std::vector<GameProjectConfig> DiscoverProjects(const std::string& rootDir = "projects");

        // Resolves a project by slug, directory path, or .hproject file path and normalizes projectRootDir
        static bool LoadProjectFromInput(
            const std::string& projectInput,
            GameProjectConfig& outConfig,
            std::string* outError = nullptr);

        // Resolves a scene/script/level path against CWD and config.projectRootDir
        static std::string ResolveProjectAssetPath(const GameProjectConfig& config, const std::string& assetPath);

        // Opens a native file dialog (Windows) for selecting a .hproject or .hscene file
        static bool BrowseForProjectFile(std::string& outFilePath);
        static bool BrowseForSceneFile(std::string& outFilePath);

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
