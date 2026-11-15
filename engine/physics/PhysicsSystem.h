#pragma once
#include "../scene/Scene.h"
#include <vector>
#include <cstdint>

namespace Hargon
{
    struct CollisionEvent
    {
        EntityID entityA = kInvalidEntity;
        EntityID entityB = kInvalidEntity;
        glm::vec3 contactPoint{ 0.0f };
        glm::vec3 normal{ 0.0f, 1.0f, 0.0f };
        bool isTrigger = false;
    };

    struct PhysicsRaycastHit
    {
        bool hit = false;
        EntityID entityId = kInvalidEntity;
        float distance = 0.0f;
        glm::vec3 point{ 0.0f };
        glm::vec3 normal{ 0.0f, 1.0f, 0.0f };
    };

    class PhysicsSystem
    {
    public:
        void Step(Scene& scene, float dt);

        // Godot PhysicsDirectSpaceState3D-inspired collision-masked 3D raycast
        static PhysicsRaycastHit Raycast(
            const Scene& scene,
            const glm::vec3& origin,
            const glm::vec3& direction,
            float maxDistance = 100.0f,
            uint32_t layerMask = 0xFFFFFFFFu,
            EntityID ignoreEntity = kInvalidEntity,
            const char* ignoreTag = nullptr);

        const std::vector<CollisionEvent>& GetCollisions() const { return m_Collisions; }

    private:
        std::vector<CollisionEvent> m_Collisions;
    };
}
