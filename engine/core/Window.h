#pragma once
#include <string>
#include <cstdint>
#include <array>
#include <glm/glm.hpp>

struct GLFWwindow;

namespace Hargon
{
    struct WindowConfig
    {
        std::string title = "Hargon 3D Game Engine";
        uint32_t width = 1600;
        uint32_t height = 900;
        bool resizable = true;
        bool vsync = true;
        bool maximized = false;
    };

    class Window
    {
    public:
        Window() = default;
        ~Window();

        bool Init(const WindowConfig& config);
        void Shutdown();

        void PollEvents();
        bool ShouldClose() const;
        void RequestClose();
        void ClearCloseRequest();

        GLFWwindow* GetNativeWindow() const { return m_Window; }
        void* GetWin32Handle() const;

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        bool WasResized() const { return m_Resized; }
        void ClearResizedFlag() { m_Resized = false; }
        bool IsFocused() const { return m_Focused; }
        float GetContentScale() const;

        void SetTitle(const std::string& title);

        // Input queries
        bool IsKeyDown(int glfwKey) const;
        bool IsKeyPressed(int glfwKey) const;
        bool IsKeyDownByName(const std::string& keyName) const;
        bool IsKeyPressedByName(const std::string& keyName) const;

        bool IsMouseButtonDown(int button) const;
        bool IsMouseButtonPressed(int button) const;
        glm::vec2 GetMousePosition() const { return m_MousePos; }
        glm::vec2 GetMouseDelta() const { return m_MouseDelta; }
        float GetScrollDelta() const { return m_ScrollDelta; }

        void SetCursorCaptured(bool captured);
        bool IsCursorCaptured() const { return m_CursorCaptured; }

        // For automated gameplay verification
        void SetSimulatedKey(int glfwKey, bool down);
        void ClearSimulatedKeys();

        static int KeyNameToGLFW(const std::string& name);

        // Internal callback helpers
        void OnResize(int width, int height);
        void OnScroll(double xoffset, double yoffset);
        void OnFocusChanged(bool focused);

    private:
        GLFWwindow* m_Window = nullptr;
        uint32_t m_Width = 1600;
        uint32_t m_Height = 900;
        bool m_Resized = false;
        bool m_Focused = true;
        bool m_CursorCaptured = false;

        std::array<bool, 512> m_KeysCurrent{};
        std::array<bool, 512> m_KeysPrevious{};
        std::array<bool, 512> m_KeysSimulated{};

        std::array<bool, 8> m_MouseCurrent{};
        std::array<bool, 8> m_MousePrevious{};

        glm::vec2 m_MousePos{ 0.0f, 0.0f };
        glm::vec2 m_LastMousePos{ 0.0f, 0.0f };
        glm::vec2 m_MouseDelta{ 0.0f, 0.0f };
        float m_ScrollDelta = 0.0f;
        float m_PendingScroll = 0.0f;
        bool m_FirstMouse = true;
    };
}
