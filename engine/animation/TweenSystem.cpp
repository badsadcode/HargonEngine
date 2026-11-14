#include "TweenSystem.h"
#include "../scene/Scene.h"
#include <cmath>
#include <algorithm>

namespace Hargon
{
    static constexpr float kPi = 3.14159265358979323846f;

    TweenSystem& TweenSystem::Get()
    {
        static TweenSystem s_Instance;
        return s_Instance;
    }

    float TweenSystem::BaseTransition(float t, TweenTransition trans)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        switch (trans)
        {
        case TweenTransition::Linear:
            return t;
        case TweenTransition::Sine:
            return 1.0f - std::cos((t * kPi) * 0.5f);
        case TweenTransition::Quad:
            return t * t;
        case TweenTransition::Cubic:
            return t * t * t;
        case TweenTransition::Quart:
            return t * t * t * t;
        case TweenTransition::Quint:
            return t * t * t * t * t;
        case TweenTransition::Expo:
            return (t <= 0.0f) ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
        case TweenTransition::Circ:
            return 1.0f - std::sqrt(std::max(0.0f, 1.0f - t * t));
        case TweenTransition::Back:
        {
            const float c1 = 1.70158f;
            const float c3 = c1 + 1.0f;
            return c3 * t * t * t - c1 * t * t;
        }
        case TweenTransition::Elastic:
        {
            if (t <= 0.0f || t >= 1.0f) return t;
            const float c4 = (2.0f * kPi) / 3.0f;
            return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
        }
        case TweenTransition::Bounce:
        {
            // Ease-in bounce is 1 - EaseOutBounce(1 - t)
            float u = 1.0f - t;
            const float n1 = 7.5625f;
            const float d1 = 2.75f;
            float b = 0.0f;
            if (u < 1.0f / d1) {
                b = n1 * u * u;
            } else if (u < 2.0f / d1) {
                u -= 1.5f / d1;
                b = n1 * u * u + 0.75f;
            } else if (u < 2.5f / d1) {
                u -= 2.25f / d1;
                b = n1 * u * u + 0.9375f;
            } else {
                u -= 2.625f / d1;
                b = n1 * u * u + 0.984375f;
            }
            return 1.0f - b;
        }
        case TweenTransition::Spring:
        {
            return 1.0f - (std::cos(t * 4.5f * kPi) * std::exp(-t * 6.0f));
        }
        default:
            return t;
        }
    }

    float TweenSystem::EvaluateCurve(float t, TweenTransition trans, TweenEase ease)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        if (trans == TweenTransition::Linear)
            return t;

        switch (ease)
        {
        case TweenEase::In:
            return BaseTransition(t, trans);
        case TweenEase::Out:
            return 1.0f - BaseTransition(1.0f - t, trans);
        case TweenEase::InOut:
            return (t < 0.5f)
                ? 0.5f * BaseTransition(t * 2.0f, trans)
                : 1.0f - 0.5f * BaseTransition((1.0f - t) * 2.0f, trans);
        case TweenEase::OutIn:
            return (t < 0.5f)
                ? 0.5f * (1.0f - BaseTransition(1.0f - t * 2.0f, trans))
                : 0.5f + 0.5f * BaseTransition((t - 0.5f) * 2.0f, trans);
        default:
            return t;
        }
    }

    static glm::vec4 ReadEntityProperty(const Entity& e, TweenProperty prop)
    {
        switch (prop)
        {
        case TweenProperty::Position:         return glm::vec4(e.transform.position, 0.0f);
        case TweenProperty::Rotation:         return glm::vec4(e.transform.rotation, 0.0f);
        case TweenProperty::Scale:            return glm::vec4(e.transform.scale, 0.0f);
        case TweenProperty::AlbedoColor:      return e.material.albedoColor;
        case TweenProperty::EmissiveStrength: return glm::vec4(e.material.emissiveStrength, 0.0f, 0.0f, 0.0f);
        case TweenProperty::LightIntensity:   return glm::vec4(e.light.intensity, 0.0f, 0.0f, 0.0f);
        default:                              return glm::vec4(0.0f);
        }
    }

    static void WriteEntityProperty(Entity& e, TweenProperty prop, const glm::vec4& v)
    {
        switch (prop)
        {
        case TweenProperty::Position:         e.transform.position = glm::vec3(v); break;
        case TweenProperty::Rotation:         e.transform.rotation = glm::vec3(v); break;
        case TweenProperty::Scale:            e.transform.scale    = glm::vec3(v); break;
        case TweenProperty::AlbedoColor:      e.material.albedoColor = v; break;
        case TweenProperty::EmissiveStrength: e.material.emissiveStrength = v.x; break;
        case TweenProperty::LightIntensity:   e.light.intensity = v.x; break;
        }
    }

    void TweenSystem::TweenEntityVec3(
        Scene& scene,
        EntityID entityId,
        TweenProperty prop,
        const glm::vec3& targetVal,
        float duration,
        TweenTransition trans,
        TweenEase ease,
        float delay,
        int loops,
        bool pingPong,
        std::function<void()> onComplete)
    {
        Entity* e = scene.GetEntity(entityId);
        if (!e) return;

        // Remove any existing tween on the same entity + property
        m_Tweens.erase(
            std::remove_if(m_Tweens.begin(), m_Tweens.end(),
                [&](const ActiveTween& tw) { return tw.targetEntity == entityId && tw.property == prop; }),
            m_Tweens.end());

        ActiveTween tw;
        tw.targetEntity   = entityId;
        tw.property       = prop;
        tw.transition     = trans;
        tw.ease           = ease;
        tw.startVal       = ReadEntityProperty(*e, prop);
        tw.endVal         = glm::vec4(targetVal, tw.startVal.w);
        tw.duration       = std::max(0.001f, duration);
        tw.elapsed        = 0.0f;
        tw.delay          = std::max(0.0f, delay);
        tw.started        = (delay <= 0.0f);
        tw.loopsRemaining = loops;
        tw.pingPong       = pingPong;
        tw.onComplete     = std::move(onComplete);
        m_Tweens.push_back(std::move(tw));
    }

    void TweenSystem::TweenEntityFloat(
        Scene& scene,
        EntityID entityId,
        TweenProperty prop,
        float targetVal,
        float duration,
        TweenTransition trans,
        TweenEase ease,
        float delay,
        std::function<void()> onComplete)
    {
        TweenEntityVec3(scene, entityId, prop, glm::vec3(targetVal, 0.0f, 0.0f), duration, trans, ease, delay, 1, false, std::move(onComplete));
    }

    void TweenSystem::Step(Scene& scene, float dt)
    {
        if (m_Tweens.empty() || dt <= 0.0f)
            return;

        for (size_t i = 0; i < m_Tweens.size();)
        {
            ActiveTween& tw = m_Tweens[i];
            Entity* e = scene.GetEntity(tw.targetEntity);
            if (!e || !e->alive)
            {
                m_Tweens.erase(m_Tweens.begin() + i);
                continue;
            }

            if (tw.delay > 0.0f)
            {
                tw.delay -= dt;
                if (tw.delay <= 0.0f && !tw.started)
                {
                    tw.started = true;
                    tw.startVal = ReadEntityProperty(*e, tw.property);
                }
                ++i;
                continue;
            }

            tw.elapsed += dt;
            float normT = std::clamp(tw.elapsed / tw.duration, 0.0f, 1.0f);
            float curvedT = EvaluateCurve(normT, tw.transition, tw.ease);
            glm::vec4 currentVal = glm::mix(tw.startVal, tw.endVal, curvedT);
            WriteEntityProperty(*e, tw.property, currentVal);

            if (tw.elapsed >= tw.duration)
            {
                if (tw.loopsRemaining < 0 || tw.loopsRemaining > 1)
                {
                    if (tw.loopsRemaining > 1)
                        --tw.loopsRemaining;
                    tw.elapsed = 0.0f;
                    if (tw.pingPong)
                        std::swap(tw.startVal, tw.endVal);
                    ++i;
                }
                else
                {
                    auto cb = std::move(tw.onComplete);
                    m_Tweens.erase(m_Tweens.begin() + i);
                    if (cb) cb();
                }
            }
            else
            {
                ++i;
            }
        }
    }

    void TweenSystem::KillTweensForEntity(EntityID entityId)
    {
        m_Tweens.erase(
            std::remove_if(m_Tweens.begin(), m_Tweens.end(),
                [&](const ActiveTween& tw) { return tw.targetEntity == entityId; }),
            m_Tweens.end());
    }

    void TweenSystem::Clear()
    {
        m_Tweens.clear();
    }
}
