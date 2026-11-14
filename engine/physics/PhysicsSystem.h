#pragma once
#include "../scene/Scene.h"
#include <vector>

namespace Hargon
{
    struct CollisionEvent
    {
        EntityID entityA = kInvalidEntity;
        EntityID entityB = kInvalidEntity;
        glm::vec3 contactPoint{ 0.0f };
        bool isTrigger = false;
    };

    class PhysicsSystem
    {
    public:
        void Step(Scene& scene, float dt);

        const std::vector<CollisionEvent>& GetCollisions() const { return m_Collisions; }

    private:
        std::vector<CollisionEvent> m_Collisions;
    };
}
