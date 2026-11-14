#include "Log.h"
#include <cstdio>
#include <chrono>

namespace Hargon
{
    std::mutex Log::s_Mutex;
    std::vector<LogEntry> Log::s_Entries;
    int Log::s_ErrorCount = 0;
    static auto s_StartTime = std::chrono::steady_clock::now();

    void Log::Init()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Entries.clear();
        s_ErrorCount = 0;
        s_StartTime = std::chrono::steady_clock::now();
    }

    static const char* CategoryToString(LogCategory cat)
    {
        switch (cat)
        {
        case LogCategory::Core:     return "CORE";
        case LogCategory::RHI:      return "RHI";
        case LogCategory::Audio:    return "AUDIO";
        case LogCategory::Lua:      return "LUA";
        case LogCategory::Physics:  return "PHYSICS";
        case LogCategory::Gameplay: return "GAME";
        case LogCategory::Editor:   return "EDITOR";
        case LogCategory::Error:    return "ERROR";
        default:                    return "LOG";
        }
    }

    void Log::Write(LogCategory category, const std::string& msg)
    {
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - s_StartTime).count();

        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            if (category == LogCategory::Error)
                ++s_ErrorCount;

            if (s_Entries.size() > 1000)
                s_Entries.erase(s_Entries.begin(), s_Entries.begin() + 200);

            s_Entries.push_back({ category, msg, elapsed });
        }

        std::printf("[%.2fs][%s] %s\n", elapsed, CategoryToString(category), msg.c_str());
        std::fflush(stdout);
    }

    std::vector<LogEntry> Log::GetEntries()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        return s_Entries;
    }

    void Log::Clear()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Entries.clear();
    }

    int Log::GetErrorCount()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        return s_ErrorCount;
    }
}
