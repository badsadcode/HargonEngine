#include "ProjectManager.h"
#include "SceneTemplates.h"
#include "../core/Log.h"
#include <filesystem>
#include <algorithm>

namespace Hargon
{
    void ProjectManager::EnsureBuiltInProjects()
    {
        std::filesystem::create_directories("projects");

        // 1. Project 1: Elder Slipgate (Single-Player FPS + BadLevel)
        {
            std::filesystem::path dir = "projects/ElderSlipgate";
            std::filesystem::path projFile = dir / "project.hproject";
            std::filesystem::create_directories(dir / "scenes");
            if (!std::filesystem::exists(projFile))
            {
                GameProjectConfig cfg;
                cfg.projectName = "ElderSlipgate";
                cfg.gameTitle = "Elder Slipgate: Quake-Style Single-Player FPS";
                cfg.genre = "Single-Player Retro FPS (BadLevel)";
                cfg.author = "Hargon Studio";
                cfg.version = "2.0.0";
                cfg.projectRootDir = "projects/ElderSlipgate";
                cfg.startupScene = "projects/ElderSlipgate/scenes/elder_slipgate.hscene";
                cfg.executableName = "ElderSlipgateFPS.exe";
                cfg.gameplayModule = "ElderSlipgateFPS";
                cfg.enableBadLevel = true;
                cfg.windowWidth = 1440;
                cfg.windowHeight = 850;
                cfg.preferredBackend = "Vulkan";
                SceneSerializer::SaveProjectConfig(cfg, projFile.string());
            }
        }

        // 2. Project 2: Aether Realms: Skyward Sanctum (3D Floating-Island Action-Platformer)
        {
            std::filesystem::path dir = "projects/AetherRealms3D";
            std::filesystem::path projFile = dir / "project.hproject";
            std::filesystem::path sceneFile = dir / "scenes" / "aether_realms.hscene";
            std::filesystem::create_directories(dir / "scenes");
            if (!std::filesystem::exists(sceneFile))
            {
                Scene s;
                SceneTemplates::BuildAetherRealmsScene(s);
                SceneSerializer::SaveToFile(s, sceneFile.string());
            }
            if (!std::filesystem::exists(projFile))
            {
                GameProjectConfig cfg;
                cfg.projectName = "AetherRealms3D";
                cfg.gameTitle = "Aether Realms: Skyward Sanctum";
                cfg.genre = "3D Floating-Island Action-Adventure & Platformer";
                cfg.author = "Hargon Studio";
                cfg.version = "1.0.0";
                cfg.projectRootDir = "projects/AetherRealms3D";
                cfg.startupScene = sceneFile.generic_string();
                cfg.executableName = "AetherRealms3D.exe";
                cfg.gameplayModule = "SceneAndLua";
                cfg.enableBadLevel = false;
                cfg.windowWidth = 1440;
                cfg.windowHeight = 850;
                cfg.preferredBackend = "Vulkan";
                cfg.maxWaves = 1;
                SceneSerializer::SaveProjectConfig(cfg, projFile.string());
            }
        }

        // 3. Project 3: Starforge Outpost: Crystal Defense (Sci-Fi Hover-Rover Turret Defense)
        {
            std::filesystem::path dir = "projects/StarforgeOutpost";
            std::filesystem::path projFile = dir / "project.hproject";
            std::filesystem::path sceneFile = dir / "scenes" / "starforge_outpost.hscene";
            std::filesystem::create_directories(dir / "scenes");
            if (!std::filesystem::exists(sceneFile))
            {
                Scene s;
                SceneTemplates::BuildStarforgeOutpostScene(s);
                SceneSerializer::SaveToFile(s, sceneFile.string());
            }
            if (!std::filesystem::exists(projFile))
            {
                GameProjectConfig cfg;
                cfg.projectName = "StarforgeOutpost";
                cfg.gameTitle = "Starforge Outpost: Crystal Defense";
                cfg.genre = "Sci-Fi Hover-Rover Action & Wave Defense";
                cfg.author = "Hargon Studio";
                cfg.version = "1.0.0";
                cfg.projectRootDir = "projects/StarforgeOutpost";
                cfg.startupScene = sceneFile.generic_string();
                cfg.executableName = "StarforgeOutpost.exe";
                cfg.gameplayModule = "SceneAndLua";
                cfg.enableBadLevel = false;
                cfg.windowWidth = 1440;
                cfg.windowHeight = 850;
                cfg.preferredBackend = "Vulkan";
                cfg.maxWaves = 3;
                SceneSerializer::SaveProjectConfig(cfg, projFile.string());
            }
        }
    }

    std::vector<GameProjectConfig> ProjectManager::DiscoverProjects(const std::string& rootDir)
    {
        EnsureBuiltInProjects();
        std::vector<GameProjectConfig> list;
        if (!std::filesystem::exists(rootDir))
            return list;

        for (const auto& entry : std::filesystem::directory_iterator(rootDir))
        {
            if (!entry.is_directory())
                continue;
            std::filesystem::path pfile = entry.path() / "project.hproject";
            if (std::filesystem::exists(pfile))
            {
                GameProjectConfig cfg;
                if (SceneSerializer::LoadProjectConfig(cfg, pfile.string()))
                    list.push_back(cfg);
            }
        }

        std::sort(list.begin(), list.end(), [](const GameProjectConfig& a, const GameProjectConfig& b) {
            return a.projectName < b.projectName;
        });
        return list;
    }

    bool ProjectManager::CreateNewProject(
        const std::string& projectSlug,
        const std::string& gameTitle,
        const std::string& genre,
        int templateType,
        bool enableBadLevel,
        GameProjectConfig& outConfig,
        std::string* outStatus)
    {
        std::string safeSlug = projectSlug.empty() ? "MyNewGame" : projectSlug;
        for (char& c : safeSlug)
        {
            if (c == ' ' || c == '/' || c == '\\' || c == ':')
                c = '_';
        }

        std::filesystem::path dir = std::filesystem::path("projects") / safeSlug;
        std::filesystem::path scenesDir = dir / "scenes";
        std::filesystem::create_directories(scenesDir);

        std::filesystem::path sceneFile = scenesDir / (safeSlug + "_main.hscene");
        Scene initialScene;
        if (templateType == 1)
            SceneTemplates::BuildAetherRealmsScene(initialScene);
        else if (templateType == 2)
            SceneTemplates::BuildStarforgeOutpostScene(initialScene);
        else if (templateType == 4)
            SceneTemplates::BuildSkywayCourseScene(initialScene, gameTitle);
        else
            SceneTemplates::BuildEmptyTemplateScene(initialScene, gameTitle);

        initialScene.sceneName = gameTitle;
        initialScene.introBannerText = "ENTERING " + gameTitle;
        initialScene.hudBannerText = initialScene.introBannerText;
        SceneSerializer::SaveToFile(initialScene, sceneFile.string());

        outConfig = {};
        outConfig.projectName = safeSlug;
        outConfig.gameTitle = gameTitle.empty() ? safeSlug : gameTitle;
        outConfig.genre = genre.empty() ? "3D Action / Adventure" : genre;
        outConfig.author = "Hargon Creator";
        outConfig.version = "0.1.0";
        outConfig.projectRootDir = dir.generic_string();
        outConfig.startupScene = sceneFile.generic_string();
        outConfig.executableName = safeSlug + ".exe";
        outConfig.gameplayModule = (templateType == 3) ? "ElderSlipgateFPS" : "SceneAndLua";
        outConfig.enableBadLevel = enableBadLevel || (templateType == 3);
        outConfig.windowWidth = 1440;
        outConfig.windowHeight = 850;
        outConfig.preferredBackend = "Vulkan";

        std::filesystem::path projFile = dir / "project.hproject";
        bool ok = SceneSerializer::SaveProjectConfig(outConfig, projFile.string());
        if (outStatus)
        {
            *outStatus = ok
                ? ("Created new project '" + outConfig.gameTitle + "' at " + projFile.generic_string())
                : "Failed to save project.hproject";
        }
        Log::Info("ProjectManager created project: " + projFile.generic_string());
        return ok;
    }
}
