#include "Window.h"
#include "Log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <algorithm>
#include <cctype>

namespace Hargon
{
    static void FramebufferResizeCallback(GLFWwindow* win, int w, int h)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (self)
            self->OnResize(w, h);
    }

    static void ScrollCallback(GLFWwindow* win, double xoff, double yoff)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (self)
            self->OnScroll(xoff, yoff);
    }

    static void FocusCallback(GLFWwindow* win, int focused)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (self)
            self->OnFocusChanged(focused == GLFW_TRUE);
    }

    Window::~Window()
    {
        Shutdown();
    }

    bool Window::Init(const WindowConfig& config)
    {
        if (!glfwInit())
        {
            Log::Error("Failed to initialize GLFW!");
            return false;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
        if (config.maximized)
            glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

        m_Width = config.width;
        m_Height = config.height;

        m_Window = glfwCreateWindow(static_cast<int>(m_Width), static_cast<int>(m_Height), config.title.c_str(), nullptr, nullptr);
        if (!m_Window)
        {
            Log::Error("Failed to create GLFW window!");
            glfwTerminate();
            return false;
        }

        glfwSetWindowUserPointer(m_Window, this);
        glfwSetFramebufferSizeCallback(m_Window, FramebufferResizeCallback);
        glfwSetScrollCallback(m_Window, ScrollCallback);
        glfwSetWindowFocusCallback(m_Window, FocusCallback);
        m_Focused = (glfwGetWindowAttrib(m_Window, GLFW_FOCUSED) == GLFW_TRUE);

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(m_Window, &fbW, &fbH);
        if (fbW > 0 && fbH > 0)
        {
            m_Width = static_cast<uint32_t>(fbW);
            m_Height = static_cast<uint32_t>(fbH);
        }

        Log::Info("GLFW Window initialized (" + std::to_string(m_Width) + "x" + std::to_string(m_Height) + "): " + config.title);
        return true;
    }

    void Window::Shutdown()
    {
        if (m_Window)
        {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
            glfwTerminate();
        }
    }

    void Window::PollEvents()
    {
        m_KeysPrevious = m_KeysCurrent;
        m_MousePrevious = m_MouseCurrent;
        m_ScrollDelta = m_PendingScroll;
        m_PendingScroll = 0.0f;

        glfwPollEvents();

        for (int k = 32; k <= GLFW_KEY_LAST && k < 512; ++k)
        {
            bool phys = (glfwGetKey(m_Window, k) == GLFW_PRESS);
            m_KeysCurrent[k] = phys || m_KeysSimulated[k];
        }

        for (int b = 0; b < 8; ++b)
        {
            m_MouseCurrent[b] = (glfwGetMouseButton(m_Window, b) == GLFW_PRESS);
        }

        double mx = 0.0, my = 0.0;
        glfwGetCursorPos(m_Window, &mx, &my);
        m_MousePos = glm::vec2(static_cast<float>(mx), static_cast<float>(my));

        if (m_FirstMouse)
        {
            m_LastMousePos = m_MousePos;
            m_FirstMouse = false;
        }

        m_MouseDelta = m_MousePos - m_LastMousePos;
        m_LastMousePos = m_MousePos;
    }

    bool Window::ShouldClose() const
    {
        return m_Window ? glfwWindowShouldClose(m_Window) != 0 : true;
    }

    void Window::RequestClose()
    {
        if (m_Window)
            glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
    }

    void Window::ClearCloseRequest()
    {
        if (m_Window)
            glfwSetWindowShouldClose(m_Window, GLFW_FALSE);
    }

    float Window::GetContentScale() const
    {
        if (!m_Window)
            return 1.0f;
        float xs = 1.0f, ys = 1.0f;
        glfwGetWindowContentScale(m_Window, &xs, &ys);
        return std::max(0.75f, std::max(xs, ys));
    }

    void* Window::GetWin32Handle() const
    {
#ifdef _WIN32
        return m_Window ? glfwGetWin32Window(m_Window) : nullptr;
#else
        return nullptr;
#endif
    }

    void Window::SetTitle(const std::string& title)
    {
        if (m_Window)
            glfwSetWindowTitle(m_Window, title.c_str());
    }

    bool Window::IsKeyDown(int glfwKey) const
    {
        if (glfwKey < 0 || glfwKey >= 512) return false;
        return m_KeysCurrent[glfwKey];
    }

    bool Window::IsKeyPressed(int glfwKey) const
    {
        if (glfwKey < 0 || glfwKey >= 512) return false;
        return m_KeysCurrent[glfwKey] && !m_KeysPrevious[glfwKey];
    }

    bool Window::IsKeyDownByName(const std::string& keyName) const
    {
        return IsKeyDown(KeyNameToGLFW(keyName));
    }

    bool Window::IsKeyPressedByName(const std::string& keyName) const
    {
        return IsKeyPressed(KeyNameToGLFW(keyName));
    }

    bool Window::IsMouseButtonDown(int button) const
    {
        if (button < 0 || button >= 8) return false;
        return m_MouseCurrent[button];
    }

    bool Window::IsMouseButtonPressed(int button) const
    {
        if (button < 0 || button >= 8) return false;
        return m_MouseCurrent[button] && !m_MousePrevious[button];
    }

    void Window::SetCursorCaptured(bool captured)
    {
        if (captured && !m_Focused)
            captured = false;
        if (m_CursorCaptured == captured)
            return;
        m_CursorCaptured = captured;
        if (m_Window)
        {
            glfwSetInputMode(m_Window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            m_FirstMouse = true;
        }
    }

    void Window::SetSimulatedKey(int glfwKey, bool down)
    {
        if (glfwKey >= 0 && glfwKey < 512)
            m_KeysSimulated[glfwKey] = down;
    }

    void Window::ClearSimulatedKeys()
    {
        m_KeysSimulated.fill(false);
    }

    int Window::KeyNameToGLFW(const std::string& name)
    {
        if (name.empty()) return -1;
        std::string upper = name;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

        if (upper.size() == 1)
        {
            char c = upper[0];
            if (c >= 'A' && c <= 'Z') return GLFW_KEY_A + (c - 'A');
            if (c >= '0' && c <= '9') return GLFW_KEY_0 + (c - '0');
        }

        if (upper == "SPACE") return GLFW_KEY_SPACE;
        if (upper == "SHIFT" || upper == "LSHIFT") return GLFW_KEY_LEFT_SHIFT;
        if (upper == "RSHIFT") return GLFW_KEY_RIGHT_SHIFT;
        if (upper == "CTRL" || upper == "LCTRL") return GLFW_KEY_LEFT_CONTROL;
        if (upper == "ALT" || upper == "LALT") return GLFW_KEY_LEFT_ALT;
        if (upper == "ENTER" || upper == "RETURN") return GLFW_KEY_ENTER;
        if (upper == "ESCAPE" || upper == "ESC") return GLFW_KEY_ESCAPE;
        if (upper == "TAB") return GLFW_KEY_TAB;
        if (upper == "UP") return GLFW_KEY_UP;
        if (upper == "DOWN") return GLFW_KEY_DOWN;
        if (upper == "LEFT") return GLFW_KEY_LEFT;
        if (upper == "RIGHT") return GLFW_KEY_RIGHT;
        if (upper == "F1") return GLFW_KEY_F1;
        if (upper == "F2") return GLFW_KEY_F2;
        if (upper == "F3") return GLFW_KEY_F3;
        if (upper == "F4") return GLFW_KEY_F4;
        if (upper == "F5") return GLFW_KEY_F5;
        if (upper == "F6") return GLFW_KEY_F6;
        if (upper == "F7") return GLFW_KEY_F7;
        if (upper == "F8") return GLFW_KEY_F8;
        if (upper == "F9") return GLFW_KEY_F9;
        if (upper == "F10") return GLFW_KEY_F10;
        if (upper == "F11") return GLFW_KEY_F11;
        if (upper == "F12") return GLFW_KEY_F12;
        if (upper == "DELETE" || upper == "DEL") return GLFW_KEY_DELETE;

        return -1;
    }

    void Window::OnResize(int width, int height)
    {
        if (width > 0 && height > 0)
        {
            m_Width = static_cast<uint32_t>(width);
            m_Height = static_cast<uint32_t>(height);
            m_Resized = true;
        }
    }

    void Window::OnScroll(double /*xoffset*/, double yoffset)
    {
        m_PendingScroll += static_cast<float>(yoffset);
    }

    void Window::OnFocusChanged(bool focused)
    {
        m_Focused = focused;
        if (!focused && m_CursorCaptured)
            SetCursorCaptured(false);
    }
}
