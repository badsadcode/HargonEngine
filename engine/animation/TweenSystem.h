#pragma once
#include "../scene/Components.h"
#include <vector>
#include <functional>
#include <glm/glm.hpp>

namespace Hargon
{
    class Scene;

    // Inspired by Godot Engine's scene/animation/tween.h
    enum class TweenTransition
    {
        Linear = 0,
        Sine = 1,
        Quad = 2,
        Cubic = 3,
        Quart = 4,
        Quint = 5,
        Expo = 6,
        Elastic = 7,
        Circ = 8,
        Bounce = 9,
        Back = 10,
        Spring = 11
    };

    enum class TweenEase
    {
        In = 0,
        Out = 1,
        InOut = 2,
        OutIn = 3
    };

    enum class TweenProperty
    {
        Position = 0,
        Rotation = 1,
        Scale = 2,
        AlbedoColor = 3,
        EmissiveStrength = 4,
        LightIntensity = 5
    };

    struct ActiveTween
    {
        EntityID targetEntity = kInvalidEntity;
        TweenProperty property = TweenProperty::Position;
        TweenTransition transition = TweenTransition::Cubic;
        TweenEase ease = TweenEase::Out;

        glm::vec4 startVal{ 0.0f };
        glm::vec4 endVal{ 0.0f };
        float duration = 1.0f;
        float elapsed = 0.0f;
        float delay = 0.0f;
        bool started = false;
        bool pingPong = false;
        int loopsRemaining = 1; // -1 for infinite
        std::function<void()> onComplete;
    };

    class TweenSystem
    {
    public:
        static TweenSystem& Get();

        // Evaluate normalized t in [0..1] using Godot's transition & ease equations
        static float EvaluateCurve(float t, TweenTransition trans, TweenEase ease);

        void TweenEntityVec3(
            Scene& scene,
            EntityID entityId,
            TweenProperty prop,
            const glm::vec3& targetVal,
            float duration,
            TweenTransition trans = TweenTransition::Cubic,
            TweenEase ease = TweenEase::Out,
            float delay = 0.0f,
            int loops = 1,
            bool pingPong = false,
            std::function<void()> onComplete = nullptr);

        void TweenEntityFloat(
            Scene& scene,
            EntityID entityId,
            TweenProperty prop,
            float targetVal,
            float duration,
            TweenTransition trans = TweenTransition::Cubic,
            TweenEase ease = TweenEase::Out,
            float delay = 0.0f,
            std::function<void()> onComplete = nullptr);

        void Step(Scene& scene, float dt);
        void KillTweensForEntity(EntityID entityId);
        void Clear();

        size_t GetActiveTweenCount() const { return m_Tweens.size(); }

    private:
        TweenSystem() = default;
        static float BaseTransition(float t, TweenTransition trans);

        std::vector<ActiveTween> m_Tweens;
    };
}
