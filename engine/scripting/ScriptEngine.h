#pragma once
#include "../scene/Scene.h"
#include <string>
#include <functional>
#include <unordered_map>

struct lua_State;

namespace Hargon
{
    class Window;
    class AudioEngine;

    using ParticleBurstCallback = std::function<void(const glm::vec3& pos, const glm::vec3& color, int count, float speed)>;

    class ScriptEngine
    {
    public:
        ScriptEngine() = default;
        ~ScriptEngine();

        bool Init(Scene* scene, Window* window, AudioEngine* audio);
        void Shutdown();

        void SetScene(Scene* scene) { m_Scene = scene; }
        void SetParticleCallback(ParticleBurstCallback cb) { m_ParticleCallback = std::move(cb); }

        void UpdateScripts(Scene& scene, float dt);
        void DispatchCollision(Scene& scene, EntityID a, EntityID b);

        bool ReloadAllScripts(Scene& scene);
        bool ExecuteSnippet(const std::string& luaCode);

        // Godot Timer-inspired named countdown timers accessible from Lua & C++
        void CreateTimer(const std::string& name, float seconds);
        bool IsTimerReady(const std::string& name) const;
        float GetTimerLeft(const std::string& name) const;
        void ClearTimers() { m_Timers.clear(); }

        static void EnsureDefaultLuaScripts(const std::string& scriptDir = "assets/scripts");

        // Internal accessors for C functions
        Scene* GetScene() const { return m_Scene; }
        Window* GetWindow() const { return m_Window; }
        AudioEngine* GetAudio() const { return m_Audio; }
        void TriggerParticles(const glm::vec3& pos, const glm::vec3& color, int count, float speed);

    private:
        bool LoadAndCallEntityFunction(Entity& entity, const char* funcName, float arg1 = 0.0f, bool hasArg1 = false, EntityID argEntity = kInvalidEntity);
        void RegisterBindings();

    private:
        lua_State* m_L = nullptr;
        Scene* m_Scene = nullptr;
        Window* m_Window = nullptr;
        AudioEngine* m_Audio = nullptr;
        ParticleBurstCallback m_ParticleCallback;
        std::unordered_map<std::string, float> m_Timers;
    };
}

