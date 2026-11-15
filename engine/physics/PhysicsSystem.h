#pragma once
#include "../scene/Scene.h"
#include <vector>
#include <set>
#include <utility>
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
        void Reset()
        {
            m_Collisions.clear();
            m_TriggerEntered.clear();
            m_TriggerExited.clear();
            m_PrevTriggers.clear();
        }

        // Godot RayCast3D continuous node evaluation
        static void UpdateRayCastNodes(Scene& scene);

        // Godot NavigationServer3D-inspired XZ Grid A* pathfinder across static obstacles
        static std::vector<glm::vec3> FindPathGrid(
            const Scene& scene,
            const glm::vec3& start,
            const glm::vec3& goal,
            float cellSize = 1.5f,
            float agentRadius = 0.65f);

        // Godot PhysicsDirectSpaceState3D-inspired collision-masked 3D raycast
        static PhysicsRaycastHit Raycast(
            const Scene& scene,
            const glm::vec3& origin,
            const glm::vec3& direction,
            float maxDistance = 100.0f,
            uint32_t layerMask = 0xFFFFFFFFu,
            EntityID ignoreEntity = kInvalidEntity,
            const char* ignoreTag = nullptr,
            bool collideWithTriggers = false);

        const std::vector<CollisionEvent>& GetCollisions() const { return m_Collisions; }
        const std::vector<CollisionEvent>& GetTriggerEntered() const { return m_TriggerEntered; }
        const std::vector<CollisionEvent>& GetTriggerExited() const { return m_TriggerExited; }

    private:
        std::vector<CollisionEvent> m_Collisions;
        std::vector<CollisionEvent> m_TriggerEntered;
        std::vector<CollisionEvent> m_TriggerExited;
        std::set<std::pair<EntityID, EntityID>> m_PrevTriggers;
    };
}
