#include "PhysicsSystem.h"
#include <cmath>
#include <algorithm>

namespace Hargon
{
    void PhysicsSystem::Step(Scene& scene, float dt)
    {
        m_Collisions.clear();
        if (dt <= 0.0f) return;
        dt = std::min(dt, 0.05f);

        auto& entities = scene.GetEntities();
        float arenaLimit = scene.arenaHalfSize - 1.5f;

        // 1. Integrate RigidBodies
        for (auto& e : entities)
        {
            if (!e.alive || !e.rigidBody.enabled || e.rigidBody.isKinematic)
                continue;

            if (e.rigidBody.useGravity && !e.rigidBody.freezeY)
            {
                e.rigidBody.velocity.y -= 9.81f * dt;
            }

            if (e.rigidBody.drag > 0.0f)
            {
                float damping = std::exp(-e.rigidBody.drag * dt);
                e.rigidBody.velocity.x *= damping;
                e.rigidBody.velocity.z *= damping;
                if (!e.rigidBody.useGravity)
                    e.rigidBody.velocity.y *= damping;
            }

            e.transform.position += e.rigidBody.velocity * dt;

            if (e.rigidBody.freezeY)
            {
                e.rigidBody.velocity.y = 0.0f;
            }
            else if (e.transform.position.y < 0.5f)
            {
                e.transform.position.y = 0.5f;
                e.rigidBody.velocity.y = -e.rigidBody.velocity.y * e.rigidBody.bounciness;
            }

            // Keep non-projectile entities inside the arena walls
            if (e.tag.tag != "Projectile" && e.tag.tag != "EnemyProjectile")
            {
                e.transform.position.x = std::clamp(e.transform.position.x, -arenaLimit, arenaLimit);
                e.transform.position.z = std::clamp(e.transform.position.z, -arenaLimit, arenaLimit);
            }
        }

        // 2. Pairwise Collision Detection & Resolution
        const size_t count = entities.size();
        for (size_t i = 0; i < count; ++i)
        {
            Entity& a = entities[i];
            if (!a.alive || !a.collider.enabled) continue;

            float radA = a.collider.isSphere
                ? a.collider.sphereRadius
                : glm::length(a.collider.boxHalfExtents) * 0.7f;

            for (size_t j = i + 1; j < count; ++j)
            {
                Entity& b = entities[j];
                if (!b.alive || !b.collider.enabled) continue;

                // Ignore projectile vs owner
                if ((a.tag.tag == "Projectile" || a.tag.tag == "EnemyProjectile") && a.gameplay.ownerId == b.id)
                    continue;
                if ((b.tag.tag == "Projectile" || b.tag.tag == "EnemyProjectile") && b.gameplay.ownerId == a.id)
                    continue;

                // Ignore static vs static
                bool aStatic = !a.rigidBody.enabled || a.rigidBody.isKinematic;
                bool bStatic = !b.rigidBody.enabled || b.rigidBody.isKinematic;
                if (aStatic && bStatic && !a.collider.isTrigger && !b.collider.isTrigger)
                    continue;

                float radB = b.collider.isSphere
                    ? b.collider.sphereRadius
                    : glm::length(b.collider.boxHalfExtents) * 0.7f;

                glm::vec3 delta = b.transform.position - a.transform.position;
                delta.y *= 0.5f; // top-down/3D arena cylinder-sphere tolerance
                float distSq = glm::dot(delta, delta);
                float minDist = radA + radB;

                if (distSq < minDist * minDist)
                {
                    float dist = std::sqrt( std::max(distSq, 0.00001f) );
                    glm::vec3 normal = delta / dist;
                    normal.y = 0.0f;
                    if (glm::length(normal) > 0.0001f)
                        normal = glm::normalize(normal);
                    else
                        normal = glm::vec3(1.0f, 0.0f, 0.0f);

                    bool isTrigger = a.collider.isTrigger || b.collider.isTrigger;
                    glm::vec3 contact = (a.transform.position + b.transform.position) * 0.5f;

                    m_Collisions.push_back({ a.id, b.id, contact, isTrigger });

                    if (!isTrigger)
                    {
                        float penetration = minDist - dist;
                        if (!aStatic && !bStatic)
                        {
                            a.transform.position -= normal * (penetration * 0.5f);
                            b.transform.position += normal * (penetration * 0.5f);
                        }
                        else if (!aStatic)
                        {
                            a.transform.position -= normal * penetration;
                        }
                        else if (!bStatic)
                        {
                            b.transform.position += normal * penetration;
                        }
                    }
                }
            }
        }
    }
}
