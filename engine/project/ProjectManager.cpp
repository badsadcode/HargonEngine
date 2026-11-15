#include "ProjectManager.h"
#include "SceneTemplates.h"
#include "../core/Log.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace Hargon
{
    namespace
    {
        bool IsWorkspaceRoot(const std::filesystem::path& dir)
        {
            std::error_code ec;
            return std::filesystem::exists(dir / "engine", ec)
                && std::filesystem::exists(dir / "assets", ec)
                && (std::filesystem::exists(dir / "projects", ec) || std::filesystem::exists(dir / "CMakeLists.txt", ec));
        }

        bool IsStandaloneGameDir(const std::filesystem::path& dir)
        {
            std::error_code ec;
            bool hasCfg = std::filesystem::exists(dir / "project.hproject", ec)
                       || std::filesystem::exists(dir / "game.json", ec);
            return hasCfg && !std::filesystem::exists(dir / "engine", ec);
        }

        std::filesystem::path GetExecutableDirectory(const char* argv0)
        {
#ifdef _WIN32
            char exeBuf[MAX_PATH]{};
            DWORD len = GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
            if (len > 0 && len < MAX_PATH)
            {
                std::error_code ec;
                return std::filesystem::path(exeBuf).parent_path().lexically_normal();
            }
#endif
            if (argv0 && argv0[0] != '\0')
            {
                std::error_code ec;
                auto p = std::filesystem::weakly_canonical(std::filesystem::path(argv0), ec);
                if (!ec && p.has_parent_path())
                    return p.parent_path();
            }
            return std::filesystem::current_path();
        }

        std::string TrimInput(std::string s)
        {
            while (!s.empty() && (std::isspace(static_cast<unsigned char>(s.front())) || s.front() == '"'))
                s.erase(s.begin());
            while (!s.empty() && (std::isspace(static_cast<unsigned char>(s.back())) || s.back() == '"'))
                s.pop_back();
            return s;
        }

        std::string ToLowerASCII(std::string s)
        {
            for (char& c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        void NormalizeLoadedProjectConfig(const std::filesystem::path& projectFilePath, GameProjectConfig& cfg)
        {
            std::error_code ec;
            std::filesystem::path absFile = std::filesystem::weakly_canonical(projectFilePath, ec);
            std::filesystem::path projDir = (!ec ? absFile : projectFilePath).parent_path().lexically_normal();
            if (projDir.empty())
                projDir = ".";

            bool rootMissing = cfg.projectRootDir.empty() || cfg.projectRootDir == "."
                || !std::filesystem::exists(cfg.projectRootDir, ec);
            if (rootMissing)
            {
                cfg.projectRootDir = projDir.generic_string();
            }
            else
            {
                // Keep workspace-relative path clean if inside current working directory
                std::filesystem::path cur = std::filesystem::current_path(ec);
                std::filesystem::path storedAbs = std::filesystem::weakly_canonical(cfg.projectRootDir, ec);
                if (!ec && storedAbs != projDir && std::filesystem::exists(projDir, ec))
                    cfg.projectRootDir = projDir.generic_string();
                (void)cur;
            }
        }
    }

    void ProjectManager::InitializeWorkingDirectory(const char* argv0)
    {
        std::error_code ec;
        std::filesystem::path cwd = std::filesystem::current_path(ec);
        if (ec)
            return;

        if (IsStandaloneGameDir(cwd))
            return;

        std::filesystem::path exeDir = GetExecutableDirectory(argv0);
        if (!exeDir.empty() && IsStandaloneGameDir(exeDir))
        {
            std::filesystem::current_path(exeDir, ec);
            if (!ec)
                Log::Info("Resolved standalone game directory: " + exeDir.generic_string());
            return;
        }

        auto findWorkspaceUpward = [&](std::filesystem::path start) -> std::filesystem::path {
            for (int depth = 0; depth < 6 && !start.empty(); ++depth)
            {
                if (IsWorkspaceRoot(start))
                    return start;
                std::filesystem::path parent = start.parent_path();
                if (parent == start)
                    break;
                start = parent;
            }
            return {};
        };

        std::filesystem::path root = findWorkspaceUpward(cwd);
        if (root.empty() && !exeDir.empty())
            root = findWorkspaceUpward(exeDir);

        if (!root.empty() && std::filesystem::weakly_canonical(root, ec) != std::filesystem::weakly_canonical(cwd, ec))
        {
            std::filesystem::current_path(root, ec);
            if (!ec)
                Log::Info("Resolved Hargon workspace root: " + root.generic_string());
        }
    }

    void ProjectManager::EnsureBuiltInProjects()
    {
        std::error_code ec;
        if (!std::filesystem::exists("engine", ec) && !std::filesystem::exists("CMakeLists.txt", ec))
            return;

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
        if (rootDir == "projects")
            EnsureBuiltInProjects();

        std::vector<GameProjectConfig> list;
        std::error_code ec;
        if (!std::filesystem::exists(rootDir, ec))
            return list;

        for (const auto& entry : std::filesystem::directory_iterator(rootDir, ec))
        {
            if (!entry.is_directory())
                continue;
            std::filesystem::path pfile = entry.path() / "project.hproject";
            if (std::filesystem::exists(pfile, ec))
            {
                GameProjectConfig cfg;
                if (SceneSerializer::LoadProjectConfig(cfg, pfile.string()))
                {
                    NormalizeLoadedProjectConfig(pfile, cfg);
                    list.push_back(cfg);
                }
            }
        }

        std::sort(list.begin(), list.end(), [](const GameProjectConfig& a, const GameProjectConfig& b) {
            return a.projectName < b.projectName;
        });
        return list;
    }

    bool ProjectManager::LoadProjectFromInput(
        const std::string& projectInput,
        GameProjectConfig& outConfig,
        std::string* outError)
    {
        std::string cleaned = TrimInput(projectInput);
        if (cleaned.empty())
        {
            if (outError) *outError = "Project path or name is empty.";
            return false;
        }

        std::error_code ec;
        std::filesystem::path p(cleaned);

        auto tryLoadFile = [&](const std::filesystem::path& filePath) -> bool {
            GameProjectConfig cfg;
            if (!SceneSerializer::LoadProjectConfig(cfg, filePath.string()))
                return false;
            NormalizeLoadedProjectConfig(filePath, cfg);
            outConfig = cfg;
            return true;
        };

        if (std::filesystem::is_regular_file(p, ec))
        {
            if (tryLoadFile(p))
                return true;
            if (outError) *outError = "Failed to parse project configuration: " + p.generic_string();
            return false;
        }

        if (std::filesystem::is_directory(p, ec))
        {
            if (std::filesystem::exists(p / "project.hproject", ec) && tryLoadFile(p / "project.hproject"))
                return true;
            if (std::filesystem::exists(p / "game.json", ec) && tryLoadFile(p / "game.json"))
                return true;
            if (outError) *outError = "Directory does not contain project.hproject: " + p.generic_string();
            return false;
        }

        std::filesystem::path workspaceProj = std::filesystem::path("projects") / cleaned / "project.hproject";
        if (std::filesystem::exists(workspaceProj, ec) && tryLoadFile(workspaceProj))
            return true;

        auto discovered = DiscoverProjects("projects");
        std::string targetLower = ToLowerASCII(cleaned);
        for (const auto& cfg : discovered)
        {
            if (cfg.projectName == cleaned || ToLowerASCII(cfg.projectName) == targetLower || cfg.gameTitle == cleaned)
            {
                outConfig = cfg;
                return true;
            }
        }

        if (outError) *outError = "Project not found: '" + cleaned + "'";
        return false;
    }

    std::string ProjectManager::ResolveProjectAssetPath(const GameProjectConfig& config, const std::string& assetPath)
    {
        std::string cleaned = TrimInput(assetPath);
        if (cleaned.empty())
            return "";

        std::error_code ec;
        std::filesystem::path p(cleaned);
        if (std::filesystem::exists(p, ec))
            return p.lexically_normal().generic_string();

        std::vector<std::filesystem::path> candidates;
        if (!config.projectRootDir.empty())
        {
            std::filesystem::path root(config.projectRootDir);
            candidates.push_back(root / p);
            candidates.push_back(root / "scenes" / p.filename());
            candidates.push_back(root / "assets" / p);
            candidates.push_back(root / "levels" / p.filename());
        }
        candidates.push_back(std::filesystem::path("..") / p);
        candidates.push_back(std::filesystem::path("../..") / p);

        for (const auto& cand : candidates)
        {
            if (std::filesystem::exists(cand, ec))
                return cand.lexically_normal().generic_string();
        }
        return cleaned;
    }

    bool ProjectManager::BrowseForProjectFile(std::string& outFilePath)
    {
#ifdef _WIN32
        char fileBuf[MAX_PATH * 2]{};
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "Hargon Project (*.hproject;*.json)\0*.hproject;*.json\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = sizeof(fileBuf);
        ofn.lpstrTitle = "Open Hargon Project (.hproject)";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn))
        {
            outFilePath = std::filesystem::path(fileBuf).generic_string();
            return true;
        }
#else
        (void)outFilePath;
#endif
        return false;
    }

    bool ProjectManager::BrowseForSceneFile(std::string& outFilePath)
    {
#ifdef _WIN32
        char fileBuf[MAX_PATH * 2]{};
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "Hargon Scene (*.hscene)\0*.hscene\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = sizeof(fileBuf);
        ofn.lpstrTitle = "Load Hargon Scene (.hscene)";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn))
        {
            outFilePath = std::filesystem::path(fileBuf).generic_string();
            return true;
        }
#else
        (void)outFilePath;
#endif
        return false;
    }

    bool ProjectManager::BrowseSaveSceneFile(std::string& outFilePath, const std::string& initialDir)
    {
#ifdef _WIN32
        char fileBuf[MAX_PATH * 2]{ "untitled_scene.hscene" };
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "Hargon Scene (*.hscene)\0*.hscene\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = sizeof(fileBuf);
        ofn.lpstrDefExt = "hscene";
        std::string initNative;
        if (!initialDir.empty())
        {
            initNative = std::filesystem::path(initialDir).make_preferred().string();
            ofn.lpstrInitialDir = initNative.c_str();
        }
        ofn.lpstrTitle = "Save Hargon Scene As (.hscene)";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetSaveFileNameA(&ofn))
        {
            outFilePath = std::filesystem::path(fileBuf).generic_string();
            return true;
        }
#else
        (void)outFilePath;
        (void)initialDir;
#endif
        return false;
    }

    bool ProjectManager::BrowseForAssetFile(std::string& outFilePath)
    {
#ifdef _WIN32
        char fileBuf[MAX_PATH * 2]{};
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "Hargon Assets (*.obj;*.lua;*.wav;*.hprefab;*.hscene;*.png;*.jpg;*.json)\0*.obj;*.lua;*.wav;*.hprefab;*.hscene;*.png;*.jpg;*.json\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = sizeof(fileBuf);
        ofn.lpstrTitle = "Import Asset Into Active Project";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn))
        {
            outFilePath = std::filesystem::path(fileBuf).generic_string();
            return true;
        }
#else
        (void)outFilePath;
#endif
        return false;
    }

    std::string ProjectManager::GetExecutableDirectoryPath()
    {
        return GetExecutableDirectory(nullptr).generic_string();
    }

    void ProjectManager::EnsureProjectFolders(const GameProjectConfig& config)
    {
        if (config.projectRootDir.empty())
            return;
        std::error_code ec;
        std::filesystem::path root(config.projectRootDir);
        const char* folders[] = { "scenes", "scripts", "prefabs", "models", "materials", "textures", "sounds", "animations" };
        for (const char* f : folders)
            std::filesystem::create_directories(root / f, ec);
    }

    bool ProjectManager::ImportAssetIntoProject(
        const GameProjectConfig& config,
        const std::string& sourceFilePath,
        const std::string& categorySubdir,
        std::string* outProjectRelPath,
        std::string* outError)
    {
        std::error_code ec;
        std::filesystem::path src(sourceFilePath);
        if (sourceFilePath.empty() || !std::filesystem::exists(src, ec) || !std::filesystem::is_regular_file(src, ec))
        {
            if (outError) *outError = "Source asset file not found: " + sourceFilePath;
            return false;
        }

        std::string cat = categorySubdir;
        if (cat.empty())
        {
            std::string ext = src.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") cat = "models";
            else if (ext == ".lua") cat = "scripts";
            else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") cat = "sounds";
            else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") cat = "textures";
            else if (ext == ".hprefab") cat = "prefabs";
            else if (ext == ".hscene") cat = "scenes";
            else if (ext == ".hmat") cat = "materials";
            else cat = "models";
        }

        std::filesystem::path root = config.projectRootDir.empty() ? std::filesystem::path("assets") : std::filesystem::path(config.projectRootDir);
        std::filesystem::path destDir = root / cat;
        std::filesystem::create_directories(destDir, ec);
        std::filesystem::path dest = destDir / src.filename();

        auto srcCanon = std::filesystem::weakly_canonical(src, ec);
        auto destCanon = std::filesystem::weakly_canonical(dest, ec);
        if (srcCanon != destCanon)
        {
            if (!std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec) || ec)
            {
                if (outError) *outError = "Failed to copy asset: " + ec.message();
                return false;
            }
        }

        std::string relPath = (std::filesystem::path(cat) / src.filename()).generic_string();
        if (outProjectRelPath)
            *outProjectRelPath = config.projectRootDir.empty() ? dest.generic_string() : (root / relPath).generic_string();

        // Update project asset manifest (.hassets.json) with stable asset ID & source record
        try
        {
            std::filesystem::path manifestPath = root / ".hassets.json";
            nlohmann::json manifest = nlohmann::json::object();
            if (std::filesystem::exists(manifestPath, ec))
            {
                std::ifstream in(manifestPath);
                if (in)
                    manifest = nlohmann::json::parse(in, nullptr, false);
                if (!manifest.is_object())
                    manifest = nlohmann::json::object();
            }
            if (!manifest.contains("assets") || !manifest["assets"].is_object())
                manifest["assets"] = nlohmann::json::object();

            uint64_t hashVal = 14695981039346656037ull;
            for (unsigned char ch : relPath)
            {
                hashVal ^= ch;
                hashVal *= 1099511628211ull;
            }
            char idBuf[32];
            std::snprintf(idBuf, sizeof(idBuf), "ast_%016llx", static_cast<unsigned long long>(hashVal));

            manifest["assets"][relPath] = {
                {"uid", idBuf},
                {"category", cat},
                {"source", src.generic_string()},
                {"fileSize", static_cast<uint64_t>(std::filesystem::file_size(dest, ec))}
            };
            std::ofstream out(manifestPath, std::ios::out | std::ios::trunc);
            if (out)
                out << manifest.dump(2);
        }
        catch (...)
        {
        }

        Log::Editor("Imported project asset '" + src.filename().string() + "' -> " + dest.generic_string());
        return true;
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
        std::string safeSlug = TrimInput(projectSlug);
        for (char& c : safeSlug)
        {
            if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                c = '_';
        }
        while (!safeSlug.empty() && (safeSlug.front() == '.' || safeSlug.front() == '_'))
            safeSlug.erase(safeSlug.begin());

        if (safeSlug.empty() || safeSlug == "." || safeSlug == "..")
        {
            std::string err = "Project folder name cannot be empty or invalid.";
            if (outStatus) *outStatus = err;
            Log::Info("[ProjectManager] " + err);
            return false;
        }

        std::filesystem::path dir = std::filesystem::path("projects") / safeSlug;
        std::error_code ec;
        if (std::filesystem::exists(dir / "project.hproject", ec) ||
            (std::filesystem::exists(dir, ec) && std::filesystem::is_directory(dir, ec) && !std::filesystem::is_empty(dir, ec)))
        {
            std::string err = "Project '" + safeSlug + "' already exists at '" + dir.generic_string() +
                              "'. Choose a different name to prevent overwriting existing files.";
            if (outStatus) *outStatus = err;
            Log::Info("[ProjectManager] " + err);
            return false;
        }

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
        if (ok)
            EnsureProjectFolders(outConfig);
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
