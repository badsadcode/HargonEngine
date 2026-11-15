#pragma once
#include "../EditorContext.h"

namespace Hargon
{
    class ConsolePanel
    {
    public:
        void Draw(EditorContext& ctx);
        void DrawEngineSystems(EditorContext& ctx);

    private:
        int m_CategoryFilter = 0; // 0=All, 1=Core, 2=RHI, 3=Lua, 4=Physics, 5=Audio, 6=Error
        char m_SearchBuf[128] = "";
        char m_ReplBuf[256] = "print('Hargon Lua console ready!')";
        bool m_AutoScroll = true;
    };
}
