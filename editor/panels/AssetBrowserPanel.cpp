#include "AssetBrowserPanel.h"
#include "../../engine/project/ProjectManager.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace Hargon
{
    namespace
    {
        bool ContainsI(const std::string& hay, const std::string& needle)
        {
            if (needle.empty()) return true;
            auto it = std::search(hay.begin(), hay.end(), needle.begin(), needle.end(),
                [](char a, char b) {
                    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                });
            return it != hay.end();
        }

        std::string GuessCategoryForExt(const std::string& extLower)
        {
            if (extLower == ".hscene") return "scenes";
            if (extLower == ".lua") return "scripts";
            if (extLower == ".hprefab") return "prefabs";
            if (extLower == ".obj" || extLower == ".gltf" || extLower == ".glb") return "models";
            if (extLower == ".hmat" || extLower == ".mat") return "materials";
            if (extLower == ".png" || extLower == ".jpg" || extLower == ".jpeg" || extLower == ".tga" || extLower == ".bmp") return "textures";
            if (extLower == ".wav" || extLower == ".ogg" || extLower == ".mp3") return "sounds";
            if (extLower == ".hanim") return "animations";
            return "models";
        }

        std::string ToLowerStr(std::string s)
        {
            for (char& ch : s)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return s;
        }
    }

    void AssetBrowserPanel::LoadManifestUIDs(const EditorContext& ctx)
    {
        m_UidMap.clear();
        if (!ctx.projectConfig || ctx.projectConfig->projectRootDir.empty())
            return;

        fs::path manifestPath = fs::path(ctx.projectConfig->projectRootDir) / ".hassets.json";
        std::ifstream ifs(manifestPath);
        if (!ifs.is_open()) return;
        try
        {
            nlohmann::json j;
            ifs >> j;
            if (j.contains("assets") && j["assets"].is_object())
            {
                for (auto it = j["assets"].begin(); it != j["assets"].end(); ++it)
                {
                    if (it.value().is_object() && it.value().contains("uid"))
                        m_UidMap.emplace_back(it.key(), it.value()["uid"].get<std::string>());
                }
            }
        }
        catch (...) {}
    }

    void AssetBrowserPanel::RefreshEntries(const EditorContext& ctx)
    {
        m_Entries.clear();
        m_SelectedIdx = -1;
        m_NeedsRefresh = false;

        static const char* kCategories[] = { "scenes", "scripts", "prefabs", "models", "materials", "textures", "sounds", "animations" };

        if (ctx.projectConfig && !ctx.projectConfig->projectRootDir.empty())
        {
            ProjectManager::EnsureProjectFolders(*ctx.projectConfig);
            fs::path projRoot(ctx.projectConfig->projectRootDir);
            std::error_code ec;

            for (const char* cat : kCategories)
            {
                fs::path subDir = projRoot / cat;
                if (!fs::exists(subDir, ec)) continue;
                for (const auto& entry : fs::recursive_directory_iterator(subDir, ec))
                {
                    if (!entry.is_regular_file()) continue;
                    AssetEntry ae;
                    ae.name = entry.path().filename().string();
                    ae.fullPath = entry.path().generic_string();
                    ae.relPath = fs::relative(entry.path(), projRoot, ec).generic_string();
                    ae.extension = ToLowerStr(entry.path().extension().string());
                    ae.category = cat;
                    ae.fileSize = entry.file_size(ec);
                    ae.isShared = false;
                    m_Entries.push_back(std::move(ae));
                }
            }
        }

        if (m_ShowSharedAssets && fs::exists("assets"))
        {
            std::error_code ec;
            for (const auto& entry : fs::recursive_directory_iterator("assets", ec))
            {
                if (!entry.is_regular_file()) continue;
                AssetEntry ae;
                ae.name = entry.path().filename().string();
                ae.fullPath = entry.path().generic_string();
                ae.relPath = entry.path().generic_string();
                ae.extension = ToLowerStr(entry.path().extension().string());
                ae.category = GuessCategoryForExt(ae.extension);
                ae.fileSize = entry.file_size(ec);
                ae.isShared = true;
                m_Entries.push_back(std::move(ae));
            }
        }

        std::sort(m_Entries.begin(), m_Entries.end(), [](const AssetEntry& a, const AssetEntry& b) {
            if (a.category != b.category) return a.category < b.category;
            return a.name < b.name;
        });
        LoadManifestUIDs(ctx);
    }

    void AssetBrowserPanel::Draw(EditorContext& ctx)
    {
        if (!ctx.showAssetBrowser)
            return;

        if (!ImGui::Begin(kWinAssetBrowser, &ctx.showAssetBrowser))
        {
            ImGui::End();
            return;
        }

        std::string curProj = ctx.projectConfig ? ctx.projectConfig->projectRootDir : "";
        if (m_LastProjectDir != curProj || m_NeedsRefresh)
        {
            m_LastProjectDir = curProj;
            RefreshEntries(ctx);
        }

        // Top controls bar
        if (ImGui::Button("Import Asset..."))
        {
            if (ctx.projectConfig)
            {
                std::string picked;
                if (ProjectManager::BrowseForAssetFile(picked))
                {
                    std::string ext = ToLowerStr(fs::path(picked).extension().string());
                    std::string cat = GuessCategoryForExt(ext);
                    std::string relOut, err;
                    if (ProjectManager::ImportAssetIntoProject(*ctx.projectConfig, picked, cat, &relOut, &err))
                    {
                        ctx.SetToast("Imported asset: " + relOut);
                        m_NeedsRefresh = true;
                    }
                    else
                    {
                        ctx.SetToast("Import failed: " + err);
                    }
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("+ New Lua Script"))
            m_OpenNewScriptPopup = true;
        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
            m_NeedsRefresh = true;
        ImGui::SameLine();
        if (ImGui::Checkbox("Shared Library", &m_ShowSharedAssets))
            m_NeedsRefresh = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##AssetFilter", "Search assets...", m_SearchBuf, sizeof(m_SearchBuf));

        // Category pills
        static const char* kCatLabels[] = { "All", "Scenes", "Scripts", "Prefabs", "Models", "Materials", "Textures", "Sounds", "Animations" };
        static const char* kCatKeys[]   = { "",    "scenes", "scripts", "prefabs", "models", "materials", "textures", "sounds", "animations" };
        for (int i = 0; i < 9; ++i)
        {
            if (i > 0) ImGui::SameLine();
            bool active = (m_SelectedCategory == i);
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.78f, 1.0f));
            if (ImGui::SmallButton(kCatLabels[i]))
                m_SelectedCategory = i;
            if (active)
                ImGui::PopStyleColor();
        }

        ImGui::Separator();

        // Asset Grid / List
        float availW = ImGui::GetContentRegionAvail().x;
        int cols = std::max(2, static_cast<int>(availW / 185.0f));

        ImGui::BeginChild("AssetListArea", ImVec2(0.0f, -26.0f), false);
        if (ImGui::BeginTable("AssetGrid", cols, ImGuiTableFlags_SizingStretchSame))
        {
            int shownCount = 0;
            for (int i = 0; i < static_cast<int>(m_Entries.size()); ++i)
            {
                const AssetEntry& ae = m_Entries[i];
                if (m_SelectedCategory > 0 && ae.category != kCatKeys[m_SelectedCategory])
                    continue;
                if (!ContainsI(ae.name, m_SearchBuf) && !ContainsI(ae.relPath, m_SearchBuf))
                    continue;

                ImGui::TableNextColumn();
                ImGui::PushID(i);

                const char* typeTag = "[FILE]";
                ImVec4 tagCol(0.75f, 0.80f, 0.88f, 1.0f);
                if (ae.extension == ".hscene")      { typeTag = "[SCN]"; tagCol = ImVec4(0.35f, 0.85f, 1.0f, 1.0f); }
                else if (ae.extension == ".lua")    { typeTag = "[LUA]"; tagCol = ImVec4(0.42f, 0.92f, 0.55f, 1.0f); }
                else if (ae.extension == ".hprefab"){ typeTag = "[PFB]"; tagCol = ImVec4(0.95f, 0.78f, 0.32f, 1.0f); }
                else if (ae.extension == ".obj")    { typeTag = "[OBJ]"; tagCol = ImVec4(0.76f, 0.72f, 0.98f, 1.0f); }
                else if (ae.extension == ".wav")    { typeTag = "[WAV]"; tagCol = ImVec4(0.95f, 0.55f, 0.72f, 1.0f); }

                char tileLabel[256];
                std::snprintf(tileLabel, sizeof(tileLabel), "%s %s%s", typeTag, ae.name.c_str(), ae.isShared ? " (Lib)" : "");

                bool selected = (m_SelectedIdx == i);
                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.45f, 0.70f, 0.90f));
                ImGui::PushStyleColor(ImGuiCol_Text, tagCol);
                bool clicked = ImGui::Selectable(tileLabel, selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, 24.0f));
                ImGui::PopStyleColor();
                if (selected)
                    ImGui::PopStyleColor();

                if (clicked)
                    m_SelectedIdx = i;

                // Tooltip with UID and metadata
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                {
                    std::string uid = "(unregistered)";
                    for (const auto& kv : m_UidMap)
                    {
                        if (kv.first == ae.relPath) { uid = kv.second; break; }
                    }
                    ImGui::SetTooltip("%s\nPath: %s\nSize: %llu bytes\nUID: %s",
                        ae.name.c_str(), ae.relPath.c_str(),
                        static_cast<unsigned long long>(ae.fileSize), uid.c_str());
                }

                // Drag Source (carries project-relative path string)
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    ImGui::SetDragDropPayload("HARGON_ASSET", ae.relPath.c_str(), ae.relPath.size() + 1);
                    ImGui::Text("%s %s", typeTag, ae.relPath.c_str());
                    ImGui::EndDragDropSource();
                }

                // Double-click execution
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    if (ae.extension == ".lua" && ctx.openScriptTab)
                    {
                        ctx.openScriptTab(ae.relPath, -1);
                        ctx.showScriptEditor = true;
                    }
                    else if (ae.extension == ".hscene" && ctx.requestGuardedAction)
                    {
                        PendingUnsavedAction act;
                        act.type = PendingActionType::LoadScene;
                        act.stringArg = ae.fullPath;
                        ctx.requestGuardedAction(act);
                    }
                    else if ((ae.extension == ".hprefab" || ae.extension == ".obj") && ctx.instantiateAssetInScene)
                    {
                        glm::vec3 pos = ctx.editorCamera ? (ctx.editorCamera->position + ctx.editorCamera->forward * 7.5f) : glm::vec3(0.0f);
                        ctx.instantiateAssetInScene(ae.relPath, &pos);
                    }
                    else if (ae.extension == ".wav" && ctx.audio)
                    {
                        ctx.audio->PlaySound2D(fs::path(ae.name).stem().string(), 0.75f, 1.0f);
                    }
                }

                ImGui::PopID();
                ++shownCount;
            }
            if (shownCount == 0)
            {
                ImGui::TableNextColumn();
                ImGui::TextDisabled("No assets in this folder yet. Use Import Asset... or drag/create scripts.");
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        // Bottom Asset Info bar
        if (m_SelectedIdx >= 0 && m_SelectedIdx < static_cast<int>(m_Entries.size()))
        {
            const AssetEntry& sel = m_Entries[m_SelectedIdx];
            ImGui::TextDisabled("%s (%s, %llu B)", sel.relPath.c_str(), sel.category.c_str(), static_cast<unsigned long long>(sel.fileSize));
        }
        else
        {
            ImGui::TextDisabled("Project Folder: %s  (%zu assets)",
                ctx.projectConfig ? ctx.projectConfig->projectRootDir.c_str() : "-", m_Entries.size());
        }

        // Create New Lua Script modal
        if (m_OpenNewScriptPopup)
        {
            ImGui::OpenPopup("Create New Lua Script");
            m_OpenNewScriptPopup = false;
        }
        if (ImGui::BeginPopupModal("Create New Lua Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Script Name", m_NewScriptNameBuf, sizeof(m_NewScriptNameBuf));
            if (ImGui::Button("Create", ImVec2(110, 0)))
            {
                if (ctx.projectConfig && m_NewScriptNameBuf[0] != '\0')
                {
                    std::string stem = m_NewScriptNameBuf;
                    if (fs::path(stem).extension() == ".lua")
                        stem = fs::path(stem).stem().string();
                    fs::path dest = fs::path(ctx.projectConfig->projectRootDir) / "scripts" / (stem + ".lua");
                    std::error_code ec;
                    fs::create_directories(dest.parent_path(), ec);
                    if (!fs::exists(dest, ec))
                    {
                        std::ofstream ofs(dest);
                        ofs << "-- " << stem << ".lua\n"
                            << "function on_ready()\n"
                            << "    -- Called once when simulation begins\n"
                            << "end\n\n"
                            << "function on_update(dt)\n"
                            << "    -- Per-frame logic\n"
                            << "end\n";
                    }
                    std::string rel = "scripts/" + stem + ".lua";
                    m_NeedsRefresh = true;
                    if (ctx.openScriptTab)
                        ctx.openScriptTab(rel, 1);
                    ctx.showScriptEditor = true;
                    ctx.SetToast("Created " + rel);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::End();
    }
}
