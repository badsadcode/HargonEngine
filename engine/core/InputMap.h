#pragma once
#include "Window.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Hargon
{
    enum class InputBindingType
    {
        Key = 0,
        MouseButton = 1
    };

    struct InputBinding
    {
        InputBindingType type = InputBindingType::Key;
        int code = 0; // GLFW_KEY_* or GLFW_MOUSE_BUTTON_*
    };

    struct InputAction
    {
        std::string name;
        float deadzone = 0.2f;
        std::vector<InputBinding> bindings;
    };

    // Inspired by Godot Engine's core/input/input_map.h
    class InputMap
    {
    public:
        static InputMap& Get();

        void LoadDefaults();

        bool HasAction(const std::string& action) const;
        void AddAction(const std::string& action, float deadzone = 0.2f);
        void EraseAction(const std::string& action);
        void ClearActionBindings(const std::string& action);

        void ActionAddKey(const std::string& action, int glfwKey);
        void ActionAddMouseButton(const std::string& action, int glfwMouseBtn);

        bool IsActionPressed(const std::string& action, const Window* window) const;
        bool IsActionJustPressed(const std::string& action, const Window* window) const;
        float GetActionStrength(const std::string& action, const Window* window) const;

        // Godot-style Input.get_vector(negative_x, positive_x, negative_y, positive_y)
        glm::vec2 GetVector(
            const std::string& negativeX,
            const std::string& positiveX,
            const std::string& negativeY,
            const std::string& positiveY,
            const Window* window) const;

        const std::unordered_map<std::string, InputAction>& GetActionMap() const { return m_Actions; }
        std::unordered_map<std::string, InputAction>& GetActionMapMutable() { return m_Actions; }

        static const char* GetKeyName(int glfwKey);

    private:
        InputMap();
        std::unordered_map<std::string, InputAction> m_Actions;
    };
}
