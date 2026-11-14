#pragma once
#include <string>
#include <vector>
#include <mutex>

namespace Hargon
{
    enum class LogCategory
    {
        Core,
        RHI,
        Audio,
        Lua,
        Physics,
        Gameplay,
        Editor,
        Error
    };

    struct LogEntry
    {
        LogCategory category;
        std::string message;
        float timestamp;
    };

    class Log
    {
    public:
        static void Init();
        static void Write(LogCategory category, const std::string& msg);
        static void Info(const std::string& msg) { Write(LogCategory::Core, msg); }
        static void RHI(const std::string& msg) { Write(LogCategory::RHI, msg); }
        static void Audio(const std::string& msg) { Write(LogCategory::Audio, msg); }
        static void Lua(const std::string& msg) { Write(LogCategory::Lua, msg); }
        static void Physics(const std::string& msg) { Write(LogCategory::Physics, msg); }
        static void Gameplay(const std::string& msg) { Write(LogCategory::Gameplay, msg); }
        static void Editor(const std::string& msg) { Write(LogCategory::Editor, msg); }
        static void Error(const std::string& msg) { Write(LogCategory::Error, msg); }

        static std::vector<LogEntry> GetEntries();
        static void Clear();
        static int GetErrorCount();

    private:
        static std::mutex s_Mutex;
        static std::vector<LogEntry> s_Entries;
        static int s_ErrorCount;
    };
}
