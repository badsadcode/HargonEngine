#pragma once
#include "../scene/Scene.h"
#include "../scene/SceneSerializer.h"
#include "../physics/PhysicsSystem.h"
#include "../scripting/ScriptEngine.h"
#include "../audio/AudioEngine.h"
#include "../renderer/Renderer3D.h"
#include <imgui.h>
#include <string>

namespace Hargon
{
    class Window;

    class GameSystem
    {
    public:
        // Steps Godot-inspired AnimationPlayer tracks & CPUParticles3D emitters
        static void StepAnimationsAndParticles(
            Scene& scene,
            Renderer3D& renderer,
            float dt);

        static void StepGameplay(
            Scene& scene,
            PhysicsSystem& physics,
            ScriptEngine& scripts,
            AudioEngine& audio,
            Renderer3D& renderer,
            Window* window,
            float dt);

        static void UpdateGameCamera(const Scene& scene, CameraView& ioCamera, float dt);

        static void RenderHUD(
            Scene& scene,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize,
            ImDrawList* drawList);

        static bool ExportStandaloneGame(
            const Scene& currentScene,
            const GameProjectConfig& config,
            const std::string& exportDir,
            std::string& outStatusMessage);
    };
}
