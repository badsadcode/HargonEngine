#include "PhysicsSystem.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace Hargon
{
    namespace
    {
        struct NarrowHit
        {
            bool hit = false;
            glm::vec3 normalAtoB{ 0.0f, 1.0f, 0.0f };
            float penetration = 0.0f;
            glm::vec3 contact{ 0.0f };
        };

        NarrowHit IntersectSphereSphere(const Entity& a, const Entity& b)
        {
            NarrowHit res;
            glm::vec3 delta = b.transform.position - a.transform.position;
            bool bothFlat = a.rigidBody.freezeY && b.rigidBody.freezeY;
            if (bothFlat)
                delta.y *= 0.5f;

            float minDist = a.collider.sphereRadius + b.collider.sphereRadius;
            float distSq = glm::dot(delta, delta);
            if (distSq >= minDist * minDist)
                return res;

            float dist = std::sqrt(std::max(distSq, 0.00001f));
            glm::vec3 n = delta / dist;
            if (bothFlat)
            {
                n.y = 0.0f;
                n = (glm::length(n) > 0.0001f) ? glm::normalize(n) : glm::vec3(1.0f, 0.0f, 0.0f);
            }
            res.hit = true;
            res.normalAtoB = n;
            res.penetration = minDist - dist;
            res.contact = a.transform.position + n * (a.collider.sphereRadius - res.penetration * 0.5f);
            return res;
        }

        NarrowHit IntersectSphereAABB(const Entity& sphereEnt, const Entity& boxEnt)
        {
            NarrowHit res;
            const glm::vec3& c = sphereEnt.transform.position;
            const glm::vec3& bc = boxEnt.transform.position;
            const glm::vec3& he = boxEnt.collider.boxHalfExtents;
            float r = sphereEnt.collider.sphereRadius;

            glm::vec3 bMin = bc - he;
            glm::vec3 bMax = bc + he;
            glm::vec3 closest = glm::clamp(c, bMin, bMax);
            glm::vec3 diff = c - closest; // points from box -> sphere
            float distSq = glm::dot(diff, diff);

            if (distSq >= r * r)
                return res;

            glm::vec3 normalBoxToSphere(0.0f, 1.0f, 0.0f);
            float pen = 0.0f;

            if (distSq > 0.00001f)
            {
                float dist = std::sqrt(distSq);
                normalBoxToSphere = diff / dist;
                pen = r - dist;
            }
            else
            {
                // Sphere center lies inside AABB: choose shallowest escape axis
                glm::vec3 local = c - bc;
                float dx = he.x - std::abs(local.x);
                float dy = he.y - std::abs(local.y);
                float dz = he.z - std::abs(local.z);
                if (dy <= dx && dy <= dz)
                {
                    normalBoxToSphere = glm::vec3(0.0f, local.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
                    pen = dy + r;
                }
                else if (dx <= dz)
                {
                    normalBoxToSphere = glm::vec3(local.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
                    pen = dx + r;
                }
                else
                {
                    normalBoxToSphere = glm::vec3(0.0f, 0.0f, local.z >= 0.0f ? 1.0f : -1.0f);
                    pen = dz + r;
                }
            }

            res.hit = true;
            res.normalAtoB = -normalBoxToSphere; // sphere -> box
            res.penetration = pen;
            res.contact = closest;
            return res;
        }

        NarrowHit IntersectAABBAABB(const Entity& a, const Entity& b)
        {
            NarrowHit res;
            glm::vec3 delta = b.transform.position - a.transform.position;
            glm::vec3 overlap = (a.collider.boxHalfExtents + b.collider.boxHalfExtents) - glm::abs(delta);
            if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f)
                return res;

            res.hit = true;
            if (overlap.y <= overlap.x && overlap.y <= overlap.z)
            {
                res.penetration = overlap.y;
                res.normalAtoB = glm::vec3(0.0f, delta.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
            }
            else if (overlap.x <= overlap.z)
            {
                res.penetration = overlap.x;
                res.normalAtoB = glm::vec3(delta.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
            }
            else
            {
                res.penetration = overlap.z;
                res.normalAtoB = glm::vec3(0.0f, 0.0f, delta.z >= 0.0f ? 1.0f : -1.0f);
            }
            res.contact = (a.transform.position + b.transform.position) * 0.5f;
            return res;
        }

        void ApplyLandingAndBounce(Entity& dynamicEnt, const glm::vec3& pushDirOnEnt)
        {
            if (dynamicEnt.rigidBody.freezeY)
                return;

            // Standing on top surface of a collider
            if (pushDirOnEnt.y > 0.65f)
            {
                dynamicEnt.rigidBody.isOnFloor = true;
                dynamicEnt.rigidBody.floorNormal = pushDirOnEnt;
                if (dynamicEnt.rigidBody.velocity.y < 0.0f)
                {
                    dynamicEnt.rigidBody.velocity.y = (dynamicEnt.rigidBody.bounciness > 0.35f)
                        ? (-dynamicEnt.rigidBody.velocity.y * dynamicEnt.rigidBody.bounciness)
                        : 0.0f;
                }
            }
            else if (pushDirOnEnt.y < -0.65f && dynamicEnt.rigidBody.velocity.y > 0.0f)
            {
                // Head bump on ceiling
                dynamicEnt.rigidBody.velocity.y = 0.0f;
            }
            else
            {
                float vn = glm::dot(dynamicEnt.rigidBody.velocity, pushDirOnEnt);
                if (vn < 0.0f)
                    dynamicEnt.rigidBody.velocity -= pushDirOnEnt * vn;
            }
        }
    }

    PhysicsRaycastHit PhysicsSystem::Raycast(
        const Scene& scene,
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance,
        uint32_t layerMask,
        EntityID ignoreEntity,
        const char* ignoreTag)
    {
        PhysicsRaycastHit best;
        float dirLen = glm::length(direction);
        if (dirLen < 0.0001f || maxDistance <= 0.0f)
            return best;

        glm::vec3 dir = direction / dirLen;
        float closestT = maxDistance;

        for (const auto& e : scene.GetEntities())
        {
            if (!e.alive || !e.collider.enabled || e.collider.isTrigger)
                continue;
            if (e.id == ignoreEntity)
                continue;
            if (ignoreTag && e.tag.tag == ignoreTag)
                continue;
            if ((e.collider.collisionLayer & layerMask) == 0u)
                continue;

            glm::vec3 center = scene.GetWorldPosition(e);

            if (e.collider.isSphere)
            {
                glm::vec3 oc = origin - center;
                float r = std::max(0.05f, e.collider.sphereRadius);
                float b = glm::dot(oc, dir);
                float c = glm::dot(oc, oc) - r * r;
                float disc = b * b - c;
                if (disc < 0.0f)
                    continue;
                float t = -b - std::sqrt(disc);
                if (t > 0.001f && t < closestT)
                {
                    closestT = t;
                    best.hit = true;
                    best.entityId = e.id;
                    best.distance = t;
                    best.point = origin + dir * t;
                    glm::vec3 n = best.point - center;
                    best.normal = (glm::length(n) > 0.0001f) ? glm::normalize(n) : glm::vec3(0.0f, 1.0f, 0.0f);
                }
            }
            else
            {
                glm::vec3 bMin = center - e.collider.boxHalfExtents;
                glm::vec3 bMax = center + e.collider.boxHalfExtents;
                float tmin = 0.001f;
                float tmax = closestT;
                glm::vec3 hitNormal(0.0f, 1.0f, 0.0f);
                bool miss = false;

                for (int axis = 0; axis < 3; ++axis)
                {
                    float invD = (std::abs(dir[axis]) > 1e-6f) ? (1.0f / dir[axis]) : 1e9f;
                    float t0 = (bMin[axis] - origin[axis]) * invD;
                    float t1 = (bMax[axis] - origin[axis]) * invD;
                    glm::vec3 n(0.0f);
                    n[axis] = (invD >= 0.0f) ? -1.0f : 1.0f;
                    if (t0 > t1)
                    {
                        std::swap(t0, t1);
                        n = -n;
                    }
                    if (t0 > tmin)
                    {
                        tmin = t0;
                        hitNormal = n;
                    }
                    tmax = std::min(tmax, t1);
                    if (tmin > tmax)
                    {
                        miss = true;
                        break;
                    }
                }

                if (!miss && tmin > 0.001f && tmin < closestT)
                {
                    closestT = tmin;
                    best.hit = true;
                    best.entityId = e.id;
                    best.distance = tmin;
                    best.point = origin + dir * tmin;
                    best.normal = hitNormal;
                }
            }
        }

        return best;
    }

    void PhysicsSystem::Step(Scene& scene, float dt)
    {
        m_Collisions.clear();
        if (dt <= 0.0f) return;
        dt = std::min(dt, 0.05f);

        auto& entities = scene.GetEntities();
        float arenaLimit = scene.arenaHalfSize - 1.5f;

        bool sceneHasPlatformColliders = false;
        for (const auto& e : entities)
        {
            if (e.alive && e.collider.enabled && !e.collider.isTrigger && !e.collider.isSphere && e.rigidBody.isKinematic)
            {
                sceneHasPlatformColliders = true;
                break;
            }
        }

        // 1. Integrate RigidBodies
        for (auto& e : entities)
        {
            if (!e.alive || !e.rigidBody.enabled || e.rigidBody.isKinematic)
                continue;

            e.rigidBody.isOnFloor = false;

            if (e.rigidBody.useGravity && !e.rigidBody.freezeY)
                e.rigidBody.velocity.y -= 18.0f * dt; // responsive platformer gravity (also respects jump arcs)

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
            else if (!sceneHasPlatformColliders && e.transform.position.y < 0.5f)
            {
                // Fallback ground plane for scenes without explicit box floor colliders
                e.transform.position.y = 0.5f;
                e.rigidBody.isOnFloor = true;
                e.rigidBody.floorNormal = glm::vec3(0.0f, 1.0f, 0.0f);
                if (e.rigidBody.velocity.y < 0.0f)
                {
                    e.rigidBody.velocity.y = (e.rigidBody.bounciness > 0.35f)
                        ? (-e.rigidBody.velocity.y * e.rigidBody.bounciness)
                        : 0.0f;
                }
            }

            // Keep non-projectile entities inside the arena walls unless arena is a wide open course
            if (e.tag.tag != "Projectile" && e.tag.tag != "EnemyProjectile" && !sceneHasPlatformColliders)
            {
                e.transform.position.x = std::clamp(e.transform.position.x, -arenaLimit, arenaLimit);
                e.transform.position.z = std::clamp(e.transform.position.z, -arenaLimit, arenaLimit);
            }
        }

        // 2. Pairwise Layer-Filtered Collision Detection & Narrowphase Resolution
        const size_t count = entities.size();
        for (size_t i = 0; i < count; ++i)
        {
            Entity& a = entities[i];
            if (!a.alive || !a.collider.enabled) continue;

            for (size_t j = i + 1; j < count; ++j)
            {
                Entity& b = entities[j];
                if (!b.alive || !b.collider.enabled) continue;

                // Godot 3D Physics layer & mask check
                bool maskOk = ((a.collider.collisionLayer & b.collider.collisionMask) != 0u) &&
                              ((b.collider.collisionLayer & a.collider.collisionMask) != 0u);
                if (!maskOk)
                    continue;

                // Ignore projectile vs owner
                if ((a.tag.tag == "Projectile" || a.tag.tag == "EnemyProjectile") && a.gameplay.ownerId == b.id)
                    continue;
                if ((b.tag.tag == "Projectile" || b.tag.tag == "EnemyProjectile") && b.gameplay.ownerId == a.id)
                    continue;

                bool aStatic = !a.rigidBody.enabled || a.rigidBody.isKinematic;
                bool bStatic = !b.rigidBody.enabled || b.rigidBody.isKinematic;
                if (aStatic && bStatic && !a.collider.isTrigger && !b.collider.isTrigger)
                    continue;

                NarrowHit hit;
                if (a.collider.isSphere && b.collider.isSphere)
                {
                    hit = IntersectSphereSphere(a, b);
                }
                else if (a.collider.isSphere && !b.collider.isSphere)
                {
                    hit = IntersectSphereAABB(a, b);
                }
                else if (!a.collider.isSphere && b.collider.isSphere)
                {
                    hit = IntersectSphereAABB(b, a);
                    hit.normalAtoB = -hit.normalAtoB;
                }
                else
                {
                    hit = IntersectAABBAABB(a, b);
                }

                if (!hit.hit)
                    continue;

                bool isTrigger = a.collider.isTrigger || b.collider.isTrigger;
                m_Collisions.push_back({ a.id, b.id, hit.contact, hit.normalAtoB, isTrigger });

                if (!isTrigger)
                {
                    glm::vec3 n = hit.normalAtoB;
                    // Respect freezeY kinematic floor interaction so freezeY arena rovers don't jitter vertically
                    if (!aStatic && a.rigidBody.freezeY && bStatic && std::abs(n.y) > 0.7f)
                        continue;
                    if (!bStatic && b.rigidBody.freezeY && aStatic && std::abs(n.y) > 0.7f)
                        continue;

                    if (!aStatic && !bStatic)
                    {
                        glm::vec3 pushA = -n * (hit.penetration * 0.5f);
                        glm::vec3 pushB =  n * (hit.penetration * 0.5f);
                        if (a.rigidBody.freezeY) pushA.y = 0.0f;
                        if (b.rigidBody.freezeY) pushB.y = 0.0f;
                        a.transform.position += pushA;
                        b.transform.position += pushB;
                        ApplyLandingAndBounce(a, -n);
                        ApplyLandingAndBounce(b,  n);
                    }
                    else if (!aStatic)
                    {
                        glm::vec3 pushA = -n * hit.penetration;
                        if (a.rigidBody.freezeY) pushA.y = 0.0f;
                        a.transform.position += pushA;
                        ApplyLandingAndBounce(a, -n);
                    }
                    else if (!bStatic)
                    {
                        glm::vec3 pushB = n * hit.penetration;
                        if (b.rigidBody.freezeY) pushB.y = 0.0f;
                        b.transform.position += pushB;
                        ApplyLandingAndBounce(b, n);
                    }
                }
            }
        }
    }
}
