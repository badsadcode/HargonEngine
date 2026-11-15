#include "ScriptEditorPanel.h"
#include "../../engine/project/ProjectManager.h"
#include "../../engine/core/Log.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace Hargon
{
    namespace
    {
        const std::unordered_set<std::string>& LuaKeywords()
        {
            static const std::unordered_set<std::string> kWords = {
                "and", "break", "do", "else", "elseif", "end", "false", "for",
                "function", "goto", "if", "in", "local", "nil", "not", "or",
                "repeat", "return", "then", "true", "until", "while"
            };
            return kWords;
        }

        const std::unordered_set<std::string>& EngineGlobals()
        {
            static const std::unordered_set<std::string> kApi = {
                "entity", "transform", "scene", "input", "audio", "physics",
                "math", "string", "table", "on_ready", "on_update",
                "on_collision", "on_trigger_enter", "on_trigger_exit",
                "print", "log_info", "emit_signal", "play_sound", "spawn_particles",
                "create_timer", "is_timer_ready", "raycast", "move_and_slide"
            };
            return kApi;
        }

        bool IsIdentStart(char c)
        {
            unsigned char u = static_cast<unsigned char>(c);
            return std::isalpha(u) || c == '_';
        }

        bool IsIdentChar(char c)
        {
            unsigned char u = static_cast<unsigned char>(c);
            return std::isalnum(u) || c == '_';
        }

        void RenderLuaLineTokens(ImDrawList* dl, ImVec2 lineStart, const char* begin, const char* end, bool& inBlockComment)
        {
            const ImU32 colDefault = IM_COL32(230, 236, 245, 255);
            const ImU32 colKeyword = IM_COL32(105, 195, 255, 255);
            const ImU32 colEngine  = IM_COL32(255, 210, 100, 255);
            const ImU32 colString  = IM_COL32(242, 168, 112, 255);
            const ImU32 colComment = IM_COL32(115, 168, 125, 255);
            const ImU32 colNumber  = IM_COL32(215, 160, 245, 255);
            const ImU32 colPunct   = IM_COL32(170, 185, 205, 255);

            float x = lineStart.x;
            float y = lineStart.y;
            const char* p = begin;

            auto emitSpan = [&](const char* s, const char* e, ImU32 col) {
                if (e <= s) return;
                dl->AddText(ImVec2(x, y), col, s, e);
                x += ImGui::CalcTextSize(s, e).x;
            };

            while (p < end)
            {
                if (inBlockComment)
                {
                    const char* close = std::strstr(p, "]]");
                    if (close && close < end)
                    {
                        emitSpan(p, close + 2, colComment);
                        p = close + 2;
                        inBlockComment = false;
                        continue;
                    }
                    emitSpan(p, end, colComment);
                    return;
                }

                if (p + 1 < end && p[0] == '-' && p[1] == '-')
                {
                    if (p + 3 < end && p[2] == '[' && p[3] == '[')
                    {
                        inBlockComment = true;
                        const char* close = std::strstr(p + 4, "]]");
                        if (close && close < end)
                        {
                            emitSpan(p, close + 2, colComment);
                            p = close + 2;
                            inBlockComment = false;
                            continue;
                        }
                    }
                    emitSpan(p, end, colComment);
                    return;
                }

                if (*p == '"' || *p == '\'')
                {
                    char quote = *p;
                    const char* q = p + 1;
                    while (q < end)
                    {
                        if (*q == '\\' && q + 1 < end) { q += 2; continue; }
                        if (*q == quote) { ++q; break; }
                        ++q;
                    }
                    emitSpan(p, q, colString);
                    p = q;
                    continue;
                }

                if (std::isdigit(static_cast<unsigned char>(*p)))
                {
                    const char* q = p + 1;
                    while (q < end && (std::isalnum(static_cast<unsigned char>(*q)) || *q == '.'))
                        ++q;
                    emitSpan(p, q, colNumber);
                    p = q;
                    continue;
                }

                if (IsIdentStart(*p))
                {
                    const char* q = p + 1;
                    while (q < end && IsIdentChar(*q))
                        ++q;
                    std::string word(p, q);
                    ImU32 col = colDefault;
                    if (LuaKeywords().count(word)) col = colKeyword;
                    else if (EngineGlobals().count(word)) col = colEngine;
                    emitSpan(p, q, col);
                    p = q;
                    continue;
                }

                // Single glyph or whitespace run
                const char* q = p + 1;
                if (*p == ' ' || *p == '\t')
                {
                    while (q < end && (*q == ' ' || *q == '\t')) ++q;
                    emitSpan(p, q, colDefault);
                }
                else
                {
                    emitSpan(p, q, colPunct);
                }
                p = q;
            }
        }

        struct LuaCallbackUserData
        {
            std::string* codePtr = nullptr;
            int* cursorPos = nullptr;
        };

        int LuaInputTextCallback(ImGuiInputTextCallbackData* data)
        {
            auto* ud = static_cast<LuaCallbackUserData*>(data->UserData);
            if (!ud) return 0;
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize && ud->codePtr)
            {
                ud->codePtr->resize(data->BufTextLen);
                data->Buf = ud->codePtr->data();
            }
            if (ud->cursorPos)
                *ud->cursorPos = data->CursorPos;
            return 0;
        }
    }

    std::string ScriptEditorPanel::ResolveDiskPath(const EditorContext& ctx, const std::string& path)
    {
        if (ctx.projectConfig)
        {
            std::string resolved = ProjectManager::ResolveProjectAssetPath(*ctx.projectConfig, path);
            if (!resolved.empty()) return resolved;
        }
        return path;
    }

    void ScriptEditorPanel::OpenScript(const EditorContext& ctx, const std::string& projectRelOrAbsPath, int jumpToLine)
    {
        if (projectRelOrAbsPath.empty())
            return;

        std::string diskPath = ResolveDiskPath(ctx, projectRelOrAbsPath);
        for (int i = 0; i < static_cast<int>(m_Tabs.size()); ++i)
        {
            if (m_Tabs[i].relPath == projectRelOrAbsPath || m_Tabs[i].diskPath == diskPath)
            {
                m_ActiveTab = i;
                if (jumpToLine > 0)
                    m_Tabs[i].pendingJumpLine = jumpToLine;
                return;
            }
        }

        ScriptTab tab;
        tab.relPath = projectRelOrAbsPath;
        tab.diskPath = diskPath;
        tab.displayName = fs::path(projectRelOrAbsPath).filename().string();
        if (tab.displayName.empty()) tab.displayName = "script.lua";
        tab.pendingJumpLine = jumpToLine;

        std::ifstream ifs(diskPath);
        if (ifs.is_open())
        {
            std::ostringstream ss;
            ss << ifs.rdbuf();
            tab.code = ss.str();
            // Normalize CRLF -> LF for consistent editor rendering & cursor math
            tab.code.erase(std::remove(tab.code.begin(), tab.code.end(), '\r'), tab.code.end());
            tab.savedCode = tab.code;
        }
        else
        {
            tab.code = "-- " + tab.displayName + "\nfunction on_ready()\nend\n\nfunction on_update(dt)\nend\n";
            tab.savedCode.clear();
        }

        RefreshDiagnostics(tab);
        m_Tabs.push_back(std::move(tab));
        m_ActiveTab = static_cast<int>(m_Tabs.size()) - 1;
    }

    bool ScriptEditorPanel::SaveTab(EditorContext& ctx, ScriptTab& tab)
    {
        if (tab.diskPath.empty())
            return false;

        std::error_code ec;
        fs::create_directories(fs::path(tab.diskPath).parent_path(), ec);
        std::ofstream ofs(tab.diskPath, std::ios::binary);
        if (!ofs.is_open())
            return false;

        ofs.write(tab.code.data(), static_cast<std::streamsize>(tab.code.size()));
        ofs.close();
        tab.savedCode = tab.code;
        if (ctx.scripts && ctx.scene)
            ctx.scripts->ReloadAllScripts(*ctx.scene);
        RefreshDiagnostics(tab);
        ctx.SetToast("Saved " + tab.displayName);
        return true;
    }

    bool ScriptEditorPanel::HasAnyDirtyTabs() const
    {
        for (const auto& t : m_Tabs)
            if (t.IsDirty()) return true;
        return false;
    }

    bool ScriptEditorPanel::SaveAllDirtyTabs(EditorContext& ctx)
    {
        bool ok = true;
        for (auto& t : m_Tabs)
        {
            if (t.IsDirty())
                ok &= SaveTab(ctx, t);
        }
        return ok;
    }

    void ScriptEditorPanel::DiscardAllDirtyTabs()
    {
        for (auto& t : m_Tabs)
            t.code = t.savedCode;
    }

    void ScriptEditorPanel::ClearAllTabs()
    {
        m_Tabs.clear();
        m_ActiveTab = -1;
    }

    void ScriptEditorPanel::RefreshDiagnostics(ScriptTab& tab)
    {
        tab.errorLine = -1;
        tab.errorMessage.clear();

        auto entries = Log::GetEntries();
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            if (it->category != LogCategory::Lua && it->category != LogCategory::Error)
                continue;
            if (it->message.find(tab.displayName) == std::string::npos)
                continue;

            // Parse pattern like "...foo.lua:14: message"
            size_t luaPos = it->message.find(tab.displayName + ":");
            if (luaPos != std::string::npos)
            {
                size_t numStart = luaPos + tab.displayName.size() + 1;
                size_t numEnd = numStart;
                while (numEnd < it->message.size() && std::isdigit(static_cast<unsigned char>(it->message[numEnd])))
                    ++numEnd;
                if (numEnd > numStart)
                {
                    tab.errorLine = std::stoi(it->message.substr(numStart, numEnd - numStart));
                    tab.errorMessage = it->message;
                    return;
                }
            }
        }
    }

    void ScriptEditorPanel::Draw(EditorContext& ctx)
    {
        if (!ctx.showScriptEditor)
            return;

        if (!ImGui::Begin(kWinScriptEditor, &ctx.showScriptEditor))
        {
            ImGui::End();
            return;
        }

        // Contextual banner showing selected entity's script WITHOUT clobbering user's open tabs
        if (ctx.scene && ctx.selectedEntity != kInvalidEntity)
        {
            if (const Entity* sel = ctx.scene->GetEntity(ctx.selectedEntity))
            {
                if (sel->HasLuaScript() && !sel->luaScript.scriptPath.empty())
                {
                    ImGui::TextDisabled("Selected (%s):", sel->tag.name.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton(("Open " + fs::path(sel->luaScript.scriptPath).filename().string()).c_str()))
                        OpenScript(ctx, sel->luaScript.scriptPath);
                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();
                }
            }
        }

        // Toolbar buttons
        bool hasActive = (m_ActiveTab >= 0 && m_ActiveTab < static_cast<int>(m_Tabs.size()));
        if (ImGui::SmallButton("Save Tab") && hasActive)
            SaveTab(ctx, m_Tabs[m_ActiveTab]);
        ImGui::SameLine();
        if (ImGui::SmallButton("Save All"))
            SaveAllDirtyTabs(ctx);
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload Scripts") && ctx.scripts && ctx.scene)
        {
            ctx.scripts->ReloadAllScripts(*ctx.scene);
            if (hasActive) RefreshDiagnostics(m_Tabs[m_ActiveTab]);
            ctx.SetToast("Reloaded Lua scripts");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Find / Replace"))
            m_ShowFindBar = !m_ShowFindBar;

        // Global script-panel keyboard shortcuts when focused
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F))
                m_ShowFindBar = true;
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_H))
                m_ShowFindBar = true;
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S) && hasActive)
                SaveTab(ctx, m_Tabs[m_ActiveTab]);
        }

        // Find / Replace bar
        if (m_ShowFindBar && hasActive)
        {
            ScriptTab& tab = m_Tabs[m_ActiveTab];
            ImGui::Separator();
            ImGui::SetNextItemWidth(170.0f);
            ImGui::InputTextWithHint("##FindText", "Find...", m_FindBuf, sizeof(m_FindBuf));
            ImGui::SameLine();
            if (ImGui::SmallButton("Next") && m_FindBuf[0] != '\0')
            {
                size_t startPos = static_cast<size_t>(std::clamp(tab.cursorPos + 1, 0, static_cast<int>(tab.code.size())));
                size_t found = tab.code.find(m_FindBuf, startPos);
                if (found == std::string::npos)
                    found = tab.code.find(m_FindBuf, 0);
                if (found != std::string::npos)
                {
                    int line = 1 + static_cast<int>(std::count(tab.code.begin(), tab.code.begin() + found, '\n'));
                    tab.pendingJumpLine = line;
                }
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(170.0f);
            ImGui::InputTextWithHint("##ReplaceText", "Replace...", m_ReplaceBuf, sizeof(m_ReplaceBuf));
            ImGui::SameLine();
            if (ImGui::SmallButton("Replace All") && m_FindBuf[0] != '\0')
            {
                std::string f(m_FindBuf), r(m_ReplaceBuf);
                size_t pos = 0;
                int count = 0;
                while ((pos = tab.code.find(f, pos)) != std::string::npos)
                {
                    tab.code.replace(pos, f.size(), r);
                    pos += r.size();
                    ++count;
                }
                ctx.SetToast("Replaced " + std::to_string(count) + " occurrences");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("x##CloseFind"))
                m_ShowFindBar = false;
        }

        ImGui::Separator();

        // Tab Bar
        if (m_Tabs.empty())
        {
            ImGui::Spacing();
            ImGui::TextWrapped("Double-click any .lua script in the Asset Browser or click 'Open in Script Editor' in the Inspector to edit Lua scripts with syntax highlighting and live hot-reload.");
            ImGui::End();
            return;
        }

        if (ImGui::BeginTabBar("LuaScriptTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs))
        {
            for (int i = 0; i < static_cast<int>(m_Tabs.size()); ++i)
            {
                ScriptTab& t = m_Tabs[i];
                bool open = true;
                std::string label = (t.IsDirty() ? "*" : "") + t.displayName + "##tab_" + t.relPath;
                if (ImGui::BeginTabItem(label.c_str(), &open))
                {
                    m_ActiveTab = i;
                    ImGui::EndTabItem();
                }
                if (!open)
                {
                    if (t.IsDirty())
                    {
                        m_PendingCloseTab = i;
                        m_OpenUnsavedCloseModal = true;
                    }
                    else
                    {
                        m_Tabs.erase(m_Tabs.begin() + i);
                        if (m_ActiveTab >= static_cast<int>(m_Tabs.size()))
                            m_ActiveTab = static_cast<int>(m_Tabs.size()) - 1;
                        --i;
                    }
                }
            }
            ImGui::EndTabBar();
        }

        if (m_ActiveTab >= 0 && m_ActiveTab < static_cast<int>(m_Tabs.size()))
        {
            ScriptTab& active = m_Tabs[m_ActiveTab];

            if (active.errorLine > 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.42f, 1.0f));
                ImGui::Text("Line %d Error: %s", active.errorLine, active.errorMessage.c_str());
                ImGui::PopStyleColor();
                ImGui::SameLine();
                if (ImGui::SmallButton("Jump to Error"))
                    active.pendingJumpLine = active.errorLine;
            }

            DrawSyntaxHighlightedEditor(active, ImVec2(0.0f, -24.0f), ctx);

            // Bottom status strip
            ImGui::TextDisabled("Line %d, Col %d  |  %s  |  %s",
                active.cursorLine, active.cursorCol,
                active.relPath.c_str(),
                active.IsDirty() ? "Modified (Unsaved)" : "Saved");
        }

        // Unsaved script tab close confirmation
        if (m_OpenUnsavedCloseModal)
        {
            ImGui::OpenPopup("Save Script Changes?");
            m_OpenUnsavedCloseModal = false;
        }
        if (ImGui::BeginPopupModal("Save Script Changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (m_PendingCloseTab >= 0 && m_PendingCloseTab < static_cast<int>(m_Tabs.size()))
            {
                ScriptTab& t = m_Tabs[m_PendingCloseTab];
                ImGui::Text("Script '%s' has unsaved edits. Save before closing tab?", t.displayName.c_str());
                if (ImGui::Button("Save", ImVec2(95, 0)))
                {
                    SaveTab(ctx, t);
                    m_Tabs.erase(m_Tabs.begin() + m_PendingCloseTab);
                    m_ActiveTab = std::min(m_ActiveTab, static_cast<int>(m_Tabs.size()) - 1);
                    m_PendingCloseTab = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Discard", ImVec2(95, 0)))
                {
                    m_Tabs.erase(m_Tabs.begin() + m_PendingCloseTab);
                    m_ActiveTab = std::min(m_ActiveTab, static_cast<int>(m_Tabs.size()) - 1);
                    m_PendingCloseTab = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(95, 0)))
                {
                    m_PendingCloseTab = -1;
                    ImGui::CloseCurrentPopup();
                }
            }
            else
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    void ScriptEditorPanel::DrawSyntaxHighlightedEditor(ScriptTab& tab, const ImVec2& availSize, EditorContext&)
    {
        float lineH = ImGui::GetFontSize();
        int totalLines = 1 + static_cast<int>(std::count(tab.code.begin(), tab.code.end(), '\n'));

        // Measure longest line for full-width child so outer scrollbar owns both X & Y
        float maxLineW = 300.0f;
        {
            const char* p = tab.code.c_str();
            while (*p)
            {
                const char* nl = std::strchr(p, '\n');
                const char* lineEnd = nl ? nl : (p + std::strlen(p));
                maxLineW = std::max(maxLineW, ImGui::CalcTextSize(p, lineEnd).x + 64.0f);
                if (!nl) break;
                p = nl + 1;
            }
        }

        ImGui::BeginChild("##LuaCodeHost", availSize, true, ImGuiWindowFlags_HorizontalScrollbar);

        float scrollTargetY = -1.0f;
        if (tab.pendingJumpLine > 0)
        {
            int targetL = std::clamp(tab.pendingJumpLine, 1, totalLines);
            scrollTargetY = std::max(0.0f, (targetL - 3) * lineH);
            tab.pendingJumpLine = -1;
        }
        if (scrollTargetY >= 0.0f)
            ImGui::SetScrollY(scrollTargetY);

        const float gutterW = 46.0f;
        float contentH = std::max(ImGui::GetContentRegionAvail().y - 4.0f, (totalLines + 2) * lineH + 16.0f);
        float contentW = std::max(ImGui::GetContentRegionAvail().x - gutterW - 8.0f, maxLineW);

        ImVec2 hostOrigin = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Line Number Gutter
        dl->AddRectFilled(
            hostOrigin,
            ImVec2(hostOrigin.x + gutterW, hostOrigin.y + contentH),
            IM_COL32(20, 24, 32, 255));
        dl->AddLine(
            ImVec2(hostOrigin.x + gutterW, hostOrigin.y),
            ImVec2(hostOrigin.x + gutterW, hostOrigin.y + contentH),
            IM_COL32(55, 65, 82, 255));

        const float padX = 6.0f;
        const float padY = 4.0f;
        for (int lineIdx = 0; lineIdx < totalLines; ++lineIdx)
        {
            float ly = hostOrigin.y + padY + lineIdx * lineH;
            if (lineIdx + 1 == tab.cursorLine)
            {
                dl->AddRectFilled(
                    ImVec2(hostOrigin.x + gutterW + 1.0f, ly),
                    ImVec2(hostOrigin.x + gutterW + contentW, ly + lineH),
                    IM_COL32(34, 46, 66, 170));
            }
            if (lineIdx + 1 == tab.errorLine)
            {
                dl->AddRectFilled(
                    ImVec2(hostOrigin.x + 2.0f, ly),
                    ImVec2(hostOrigin.x + gutterW + contentW, ly + lineH),
                    IM_COL32(140, 32, 32, 110));
            }
            char numBuf[16];
            std::snprintf(numBuf, sizeof(numBuf), "%4d", lineIdx + 1);
            dl->AddText(ImVec2(hostOrigin.x + 6.0f, ly),
                (lineIdx + 1 == tab.errorLine) ? IM_COL32(255, 115, 115, 255) : IM_COL32(120, 134, 156, 255),
                numBuf);
        }

        // Advance cursor to right of gutter for InputTextMultiline
        ImGui::SetCursorScreenPos(ImVec2(hostOrigin.x + gutterW, hostOrigin.y));

        // Render interactive InputTextMultiline with transparent text while drawing colored tokens over top
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, padY));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        if (tab.code.capacity() < tab.code.size() + 1024)
            tab.code.reserve(tab.code.size() + 2048);

        LuaCallbackUserData cbData{ &tab.code, &tab.cursorPos };
        std::string widgetId = "##LuaSrc_" + tab.relPath;
        ImGui::InputTextMultiline(
            widgetId.c_str(),
            tab.code.data(),
            tab.code.capacity() + 1,
            ImVec2(contentW, contentH),
            ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackAlways,
            LuaInputTextCallback,
            &cbData);

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        // Update cursor Line / Col
        int clampedPos = std::clamp(tab.cursorPos, 0, static_cast<int>(tab.code.size()));
        tab.cursorLine = 1 + static_cast<int>(std::count(tab.code.begin(), tab.code.begin() + clampedPos, '\n'));
        size_t lastNl = tab.code.rfind('\n', clampedPos > 0 ? static_cast<size_t>(clampedPos - 1) : 0);
        tab.cursorCol = (lastNl == std::string::npos || clampedPos == 0)
            ? (clampedPos + 1)
            : static_cast<int>(clampedPos - lastNl);

        // Draw syntax-colored tokens at exact text origin
        const char* p = tab.code.c_str();
        bool inBlockComment = false;
        int lineIdx = 0;
        ImVec2 textOrigin(hostOrigin.x + gutterW + padX, hostOrigin.y + padY);
        while (true)
        {
            const char* nl = std::strchr(p, '\n');
            const char* lineEnd = nl ? nl : (p + std::strlen(p));
            RenderLuaLineTokens(dl, ImVec2(textOrigin.x, textOrigin.y + lineIdx * lineH), p, lineEnd, inBlockComment);
            if (!nl) break;
            p = nl + 1;
            ++lineIdx;
        }

        ImGui::EndChild();
    }
}
