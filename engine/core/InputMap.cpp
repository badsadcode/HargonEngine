#include "InputMap.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>

namespace Hargon
{
    InputMap& InputMap::Get()
    {
        static InputMap s_Instance;
        return s_Instance;
    }

    InputMap::InputMap()
    {
        LoadDefaults();
    }

    void InputMap::LoadDefaults()
    {
        m_Actions.clear();

        // Standard movement & FPS gameplay actions (Godot InputMap style)
        AddAction("move_forward", 0.2f);
        ActionAddKey("move_forward", GLFW_KEY_W);
        ActionAddKey("move_forward", GLFW_KEY_UP);

        AddAction("move_backward", 0.2f);
        ActionAddKey("move_backward", GLFW_KEY_S);
        ActionAddKey("move_backward", GLFW_KEY_DOWN);

        AddAction("move_left", 0.2f);
        ActionAddKey("move_left", GLFW_KEY_A);

        AddAction("move_right", 0.2f);
        ActionAddKey("move_right", GLFW_KEY_D);

        AddAction("turn_left", 0.2f);
        ActionAddKey("turn_left", GLFW_KEY_LEFT);

        AddAction("turn_right", 0.2f);
        ActionAddKey("turn_right", GLFW_KEY_RIGHT);

        AddAction("jump", 0.2f);
        ActionAddKey("jump", GLFW_KEY_SPACE);

        AddAction("sprint", 0.2f);
        ActionAddKey("sprint", GLFW_KEY_LEFT_SHIFT);

        AddAction("fire", 0.2f);
        ActionAddMouseButton("fire", GLFW_MOUSE_BUTTON_LEFT);
        ActionAddKey("fire", GLFW_KEY_F);

        AddAction("weapon_1", 0.2f); ActionAddKey("weapon_1", GLFW_KEY_1);
        AddAction("weapon_2", 0.2f); ActionAddKey("weapon_2", GLFW_KEY_2);
        AddAction("weapon_3", 0.2f); ActionAddKey("weapon_3", GLFW_KEY_3);
        AddAction("weapon_4", 0.2f); ActionAddKey("weapon_4", GLFW_KEY_4);
        AddAction("weapon_5", 0.2f); ActionAddKey("weapon_5", GLFW_KEY_5);

        AddAction("quick_save", 0.5f); ActionAddKey("quick_save", GLFW_KEY_F5);
        AddAction("quick_load", 0.5f); ActionAddKey("quick_load", GLFW_KEY_F9);
        AddAction("ui_cancel", 0.5f);  ActionAddKey("ui_cancel", GLFW_KEY_ESCAPE);
    }

    bool InputMap::HasAction(const std::string& action) const
    {
        return m_Actions.find(action) != m_Actions.end();
    }

    void InputMap::AddAction(const std::string& action, float deadzone)
    {
        InputAction& act = m_Actions[action];
        act.name = action;
        act.deadzone = deadzone;
    }

    void InputMap::EraseAction(const std::string& action)
    {
        m_Actions.erase(action);
    }

    void InputMap::ClearActionBindings(const std::string& action)
    {
        auto it = m_Actions.find(action);
        if (it != m_Actions.end())
            it->second.bindings.clear();
    }

    void InputMap::ActionAddKey(const std::string& action, int glfwKey)
    {
        if (!HasAction(action))
            AddAction(action);
        auto& bindings = m_Actions[action].bindings;
        for (const auto& b : bindings)
        {
            if (b.type == InputBindingType::Key && b.code == glfwKey)
                return;
        }
        bindings.push_back({ InputBindingType::Key, glfwKey });
    }

    void InputMap::ActionAddMouseButton(const std::string& action, int glfwMouseBtn)
    {
        if (!HasAction(action))
            AddAction(action);
        auto& bindings = m_Actions[action].bindings;
        for (const auto& b : bindings)
        {
            if (b.type == InputBindingType::MouseButton && b.code == glfwMouseBtn)
                return;
        }
        bindings.push_back({ InputBindingType::MouseButton, glfwMouseBtn });
    }

    bool InputMap::IsActionPressed(const std::string& action, const Window* window) const
    {
        if (!window) return false;
        auto it = m_Actions.find(action);
        if (it == m_Actions.end()) return false;

        for (const auto& b : it->second.bindings)
        {
            if (b.type == InputBindingType::Key && window->IsKeyDown(b.code))
                return true;
            if (b.type == InputBindingType::MouseButton && window->IsMouseButtonDown(b.code))
                return true;
        }
        return false;
    }

    bool InputMap::IsActionJustPressed(const std::string& action, const Window* window) const
    {
        if (!window) return false;
        auto it = m_Actions.find(action);
        if (it == m_Actions.end()) return false;

        for (const auto& b : it->second.bindings)
        {
            if (b.type == InputBindingType::Key && window->IsKeyPressed(b.code))
                return true;
            if (b.type == InputBindingType::MouseButton && window->IsMouseButtonPressed(b.code))
                return true;
        }
        return false;
    }

    float InputMap::GetActionStrength(const std::string& action, const Window* window) const
    {
        return IsActionPressed(action, window) ? 1.0f : 0.0f;
    }

    glm::vec2 InputMap::GetVector(
        const std::string& negativeX,
        const std::string& positiveX,
        const std::string& negativeY,
        const std::string& positiveY,
        const Window* window) const
    {
        glm::vec2 v(
            GetActionStrength(positiveX, window) - GetActionStrength(negativeX, window),
            GetActionStrength(positiveY, window) - GetActionStrength(negativeY, window));
        float len = glm::length(v);
        if (len > 1.0f)
            v /= len;
        return v;
    }

    const char* InputMap::GetKeyName(int glfwKey)
    {
        switch (glfwKey)
        {
        case GLFW_KEY_W: return "W";
        case GLFW_KEY_A: return "A";
        case GLFW_KEY_S: return "S";
        case GLFW_KEY_D: return "D";
        case GLFW_KEY_Q: return "Q";
        case GLFW_KEY_E: return "E";
        case GLFW_KEY_R: return "R";
        case GLFW_KEY_F: return "F";
        case GLFW_KEY_SPACE: return "Space";
        case GLFW_KEY_LEFT_SHIFT: return "Left Shift";
        case GLFW_KEY_ESCAPE: return "Escape";
        case GLFW_KEY_UP: return "Up Arrow";
        case GLFW_KEY_DOWN: return "Down Arrow";
        case GLFW_KEY_LEFT: return "Left Arrow";
        case GLFW_KEY_RIGHT: return "Right Arrow";
        case GLFW_KEY_1: return "1";
        case GLFW_KEY_2: return "2";
        case GLFW_KEY_3: return "3";
        case GLFW_KEY_4: return "4";
        case GLFW_KEY_5: return "5";
        case GLFW_KEY_F5: return "F5";
        case GLFW_KEY_F9: return "F9";
        default: return "Key";
        }
    }
}
