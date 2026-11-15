#include "ConsolePanel.h"
#include "../commands/EditorCommandSystem.h"
#include "../../engine/core/Log.h"
#include "../../engine/core/InputMap.h"
#include "../../engine/animation/TweenSystem.h"
#include <algorithm>
#include <cctype>

namespace Hargon
{
    namespace
    {
        bool MatchesFilterNoCase(const std::string& hay, const std::string& needle)
        {
            if (needle.empty()) return true;
            auto it = std::search(hay.begin(), hay.end(), needle.begin(), needle.end(),
                [](char a, char b) {
                    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                });
            return it != hay.end();
        }
    }

    void ConsolePanel::Draw(EditorContext& ctx)
    {
        if (!ctx.showConsole)
            return;

        if (!ImGui::Begin(kWinConsole, &ctx.showConsole))
        {
            ImGui::End();
            return;
        }

        if (ImGui::SmallButton("Clear"))
            Log::Clear();
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_AutoScroll);
        ImGui::SameLine();
        const char* cats[] = { "All", "Core", "RHI", "Lua", "Physics", "Audio", "Errors Only" };
        ImGui::SetNextItemWidth(130.0f);
        ImGui::Combo("##LogCat", &m_CategoryFilter, cats, IM_ARRAYSIZE(cats));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##LogFilter", "Search logs...", m_SearchBuf, sizeof(m_SearchBuf));
        ImGui::SameLine();
        if (ImGui::SmallButton("Open Engine Systems"))
            ctx.showSystems = true;

        ImGui::Separator();

        ImGui::BeginChild("ConsoleLogScroll", ImVec2(0.0f, -32.0f), true);
        auto entries = Log::GetEntries();
        for (size_t i = 0; i < entries.size(); ++i)
        {
            const auto& e = entries[i];
            if (m_CategoryFilter == 1 && e.category != LogCategory::Core && e.category != LogCategory::Editor && e.category != LogCategory::Gameplay) continue;
            if (m_CategoryFilter == 2 && e.category != LogCategory::RHI) continue;
            if (m_CategoryFilter == 3 && e.category != LogCategory::Lua) continue;
            if (m_CategoryFilter == 4 && e.category != LogCategory::Physics) continue;
            if (m_CategoryFilter == 5 && e.category != LogCategory::Audio) continue;
            if (m_CategoryFilter == 6 && e.category != LogCategory::Error) continue;
            if (!MatchesFilterNoCase(e.message, m_SearchBuf)) continue;

            ImVec4 col(0.84f, 0.88f, 0.94f, 1.0f);
            const char* tag = "[INFO]";
            switch (e.category)
            {
            case LogCategory::RHI:      col = ImVec4(0.45f, 0.85f, 1.00f, 1.0f); tag = "[RHI]"; break;
            case LogCategory::Lua:      col = ImVec4(0.55f, 0.95f, 0.62f, 1.0f); tag = "[LUA]"; break;
            case LogCategory::Physics:  col = ImVec4(0.95f, 0.82f, 0.45f, 1.0f); tag = "[PHYS]"; break;
            case LogCategory::Audio:    col = ImVec4(0.85f, 0.65f, 1.00f, 1.0f); tag = "[AUDIO]"; break;
            case LogCategory::Gameplay: col = ImVec4(0.48f, 0.92f, 0.85f, 1.0f); tag = "[GAME]"; break;
            case LogCategory::Editor:   col = ImVec4(0.75f, 0.82f, 0.96f, 1.0f); tag = "[EDIT]"; break;
            case LogCategory::Error:    col = ImVec4(1.00f, 0.42f, 0.40f, 1.0f); tag = "[ERROR]"; break;
            default: break;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, col);
            char lineBuf[1024];
            std::snprintf(lineBuf, sizeof(lineBuf), "[%.2fs] %s %s##log_%zu", e.timestamp, tag, e.message.c_str(), i);
            if (ImGui::Selectable(lineBuf, false))
            {
                // If message contains "<script>.lua:<line>:", jump directly to Script Editor
                size_t luaExt = e.message.find(".lua:");
                if (luaExt != std::string::npos && ctx.openScriptTab)
                {
                    size_t start = luaExt;
                    while (start > 0 && e.message[start - 1] != ' ' && e.message[start - 1] != '[' && e.message[start - 1] != '"' && e.message[start - 1] != '\'')
                        --start;
                    std::string scriptRef = e.message.substr(start, luaExt + 4 - start);
                    int lineNum = 1;
                    size_t colonPos = luaExt + 5;
                    size_t endNum = colonPos;
                    while (endNum < e.message.size() && std::isdigit(static_cast<unsigned char>(e.message[endNum])))
                        ++endNum;
                    if (endNum > colonPos)
                        lineNum = std::stoi(e.message.substr(colonPos, endNum - colonPos));
                    ctx.openScriptTab(scriptRef, lineNum);
                    ctx.showScriptEditor = true;
                }
            }
            ImGui::PopStyleColor();
        }
        if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 8.0f)
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        // Interactive Lua REPL
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
        bool runRepl = ImGui::InputTextWithHint("##LuaRepl", "Run Lua snippet...", m_ReplBuf, sizeof(m_ReplBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Run Lua", ImVec2(84.0f, 0.0f)) || runRepl)
        {
            if (ctx.scripts && m_ReplBuf[0] != '\0')
                ctx.scripts->ExecuteSnippet(m_ReplBuf);
        }

        ImGui::End();
    }

    void ConsolePanel::DrawEngineSystems(EditorContext& ctx)
    {
        if (!ctx.showSystems)
            return;

        if (!ImGui::Begin(kWinSystems, &ctx.showSystems))
        {
            ImGui::End();
            return;
        }

        if (ImGui::CollapsingHeader("Command History (Undo / Redo)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ctx.commands)
            {
                const auto& hist = ctx.commands->GetHistory();
                int cursor = ctx.commands->GetCursor();
                int savedCursor = ctx.commands->GetSavedCursor();
                bool saveReach = ctx.commands->IsSaveReachable();

                ImGui::TextDisabled("%zu commands | Cursor: %d | Dirty: %s",
                    hist.size(), cursor, ctx.commands->IsDirty() ? "YES" : "No");
                if (hist.empty())
                {
                    ImGui::TextDisabled("No edits recorded yet.");
                }
                else
                {
                    ImGui::BeginChild("HistoryList", ImVec2(0.0f, 150.0f), true);
                    for (int i = static_cast<int>(hist.size()) - 1; i >= 0; --i)
                    {
                        bool applied = (i < cursor);
                        bool isSavePoint = (saveReach && i + 1 == savedCursor);
                        ImVec4 col = applied ? ImVec4(0.86f, 0.92f, 0.98f, 1.0f) : ImVec4(0.50f, 0.54f, 0.62f, 1.0f);
                        ImGui::TextColored(col, "%s %s%s",
                            applied ? "[Done]" : "[Redo]",
                            hist[i].description.c_str(),
                            isSavePoint ? "  <-- Saved" : "");
                    }
                    ImGui::EndChild();
                }
            }
        }

        if (ImGui::CollapsingHeader("Signal Bus & Node Groups"))
        {
            if (ctx.scene)
            {
                const auto& sigs = ctx.scene->GetRecentSignals();
                ImGui::TextDisabled("Recent Signals (%zu):", sigs.size());
                for (auto it = sigs.rbegin(); it != sigs.rend(); ++it)
                    ImGui::BulletText("%s", it->c_str());
            }
        }

        if (ImGui::CollapsingHeader("Input Action Map"))
        {
            const auto& actions = InputMap::Get().GetActionMap();
            for (const auto& [name, act] : actions)
            {
                std::string keysStr;
                for (size_t i = 0; i < act.bindings.size(); ++i)
                {
                    if (i > 0) keysStr += ", ";
                    if (act.bindings[i].type == InputBindingType::Key)
                        keysStr += InputMap::GetKeyName(act.bindings[i].code);
                    else
                        keysStr += "Mouse" + std::to_string(act.bindings[i].code);
                }
                ImGui::Text("%-16s -> %s", name.c_str(), keysStr.c_str());
            }
        }

        if (ImGui::CollapsingHeader("Active Tweens & Simulation"))
        {
            ImGui::Text("Active Tweens: %zu", TweenSystem::Get().GetActiveTweenCount());
            if (ctx.physics)
            {
                ImGui::Text("Active Collisions: %zu | Trigger Enters: %zu",
                    ctx.physics->GetCollisions().size(), ctx.physics->GetTriggerEntered().size());
            }
        }

        ImGui::End();
    }
}
