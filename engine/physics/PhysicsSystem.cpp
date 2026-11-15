#include "PhysicsSystem.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <queue>

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
        const char* ignoreTag,
        bool collideWithTriggers)
    {
        PhysicsRaycastHit best;
        float dirLen = glm::length(direction);
        if (dirLen < 0.0001f || maxDistance <= 0.0f)
            return best;

        glm::vec3 dir = direction / dirLen;
        float closestT = maxDistance;

        for (const auto& e : scene.GetEntities())
        {
            if (!e.alive || !e.collider.enabled)
                continue;
            if (e.collider.isTrigger && !collideWithTriggers)
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

    namespace
    {
        void ApplyArea3DOverride(const Entity& area, Entity& body, float dt)
        {
            if (!area.collider.isTrigger || area.collider.areaSpaceOverride <= 0)
                return;
            if (!body.rigidBody.enabled || body.rigidBody.isKinematic)
                return;

            if (area.collider.areaSpaceOverride == 1) // ReplaceGravity (low-G / updraft / zero-G zone)
            {
                if (body.rigidBody.useGravity && !body.rigidBody.freezeY)
                    body.rigidBody.velocity.y += 18.0f * dt; // cancel default world gravity
                body.rigidBody.velocity += area.collider.areaGravityVec * dt;
            }
            else if (area.collider.areaSpaceOverride == 2) // WindForce / ForceField
            {
                float invMass = 1.0f / std::max(body.rigidBody.mass, 0.1f);
                body.rigidBody.velocity += area.collider.areaForceVec * invMass * dt;
            }

            if (area.collider.areaLinearDamp > 0.0f)
            {
                float damp = std::exp(-area.collider.areaLinearDamp * dt);
                body.rigidBody.velocity *= damp;
            }
        }
    }

    void PhysicsSystem::UpdateRayCastNodes(Scene& scene)
    {
        for (auto& e : scene.GetEntities())
        {
            if (!e.alive || !e.raycast.enabled)
                continue;

            e.raycast.isColliding = false;
            e.raycast.hitEntity = kInvalidEntity;
            e.raycast.hitDistance = 0.0f;

            glm::vec3 origin = scene.GetWorldPosition(e);
            glm::vec3 worldTarget = glm::vec3(scene.GetWorldMatrix(e) * glm::vec4(e.raycast.targetPosition, 1.0f));
            glm::vec3 delta = worldTarget - origin;
            float maxDist = glm::length(delta);
            e.raycast.hitPoint = worldTarget;
            e.raycast.hitNormal = glm::vec3(0.0f, 1.0f, 0.0f);

            if (maxDist <= 0.0001f)
                continue;

            PhysicsRaycastHit hit = Raycast(
                scene, origin, delta, maxDist,
                e.raycast.collisionMask, e.id, nullptr, e.raycast.collideWithTriggers);

            if (hit.hit)
            {
                e.raycast.isColliding = true;
                e.raycast.hitEntity = hit.entityId;
                e.raycast.hitPoint = hit.point;
                e.raycast.hitNormal = hit.normal;
                e.raycast.hitDistance = hit.distance;
            }
        }
    }

    std::vector<glm::vec3> PhysicsSystem::FindPathGrid(
        const Scene& scene,
        const glm::vec3& start,
        const glm::vec3& goal,
        float cellSize,
        float agentRadius)
    {
        cellSize = std::max(cellSize, 0.5f);
        float halfSpan = std::max(scene.arenaHalfSize, std::max(std::abs(goal.x), std::abs(goal.z)) + 6.0f);
        int gridN = std::clamp(static_cast<int>(std::ceil((2.0f * halfSpan) / cellSize)), 8, 72);
        float effCell = (2.0f * halfSpan) / static_cast<float>(gridN);
        float minCoord = -halfSpan;

        auto worldToCell = [&](float wx, float wz) -> std::pair<int, int> {
            int cx = std::clamp(static_cast<int>(std::floor((wx - minCoord) / effCell)), 0, gridN - 1);
            int cz = std::clamp(static_cast<int>(std::floor((wz - minCoord) / effCell)), 0, gridN - 1);
            return { cx, cz };
        };
        auto cellToWorld = [&](int cx, int cz) -> glm::vec3 {
            return glm::vec3(minCoord + (cx + 0.5f) * effCell, start.y, minCoord + (cz + 0.5f) * effCell);
        };

        std::vector<uint8_t> blocked(static_cast<size_t>(gridN) * gridN, 0);
        for (const auto& e : scene.GetEntities())
        {
            if (!e.alive || !e.collider.enabled || e.collider.isTrigger)
                continue;
            if (e.rigidBody.enabled && !e.rigidBody.isKinematic)
                continue;

            glm::vec3 wp = scene.GetWorldPosition(e);
            float hx = e.collider.isSphere ? e.collider.sphereRadius : e.collider.boxHalfExtents.x;
            float hy = e.collider.isSphere ? e.collider.sphereRadius : e.collider.boxHalfExtents.y;
            float hz = e.collider.isSphere ? e.collider.sphereRadius : e.collider.boxHalfExtents.z;

            // Skip ground/floor slabs below feet and overhead ceilings above agent head
            if ((wp.y + hy) < (start.y + 0.15f) || (wp.y - hy) > (start.y + 1.85f))
                continue;

            int minX = std::clamp(static_cast<int>(std::floor((wp.x - hx - agentRadius - minCoord) / effCell)), 0, gridN - 1);
            int maxX = std::clamp(static_cast<int>(std::floor((wp.x + hx + agentRadius - minCoord) / effCell)), 0, gridN - 1);
            int minZ = std::clamp(static_cast<int>(std::floor((wp.z - hz - agentRadius - minCoord) / effCell)), 0, gridN - 1);
            int maxZ = std::clamp(static_cast<int>(std::floor((wp.z + hz + agentRadius - minCoord) / effCell)), 0, gridN - 1);
            for (int z = minZ; z <= maxZ; ++z)
                for (int x = minX; x <= maxX; ++x)
                    blocked[static_cast<size_t>(z) * gridN + x] = 1;
        }

        auto [sx, sz] = worldToCell(start.x, start.z);
        auto [gx, gz] = worldToCell(goal.x, goal.z);
        blocked[static_cast<size_t>(sz) * gridN + sx] = 0;
        blocked[static_cast<size_t>(gz) * gridN + gx] = 0;

        const int total = gridN * gridN;
        const int startIdx = sz * gridN + sx;
        const int goalIdx = gz * gridN + gx;
        if (startIdx == goalIdx)
            return { glm::vec3(goal.x, start.y, goal.z) };

        std::vector<float> gScore(total, std::numeric_limits<float>::infinity());
        std::vector<int> cameFrom(total, -1);
        std::vector<uint8_t> closed(total, 0);

        struct Node { float f; int idx; bool operator>(const Node& o) const { return f > o.f; } };
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

        auto heuristic = [&](int cx, int cz) {
            float dx = static_cast<float>(std::abs(cx - gx));
            float dz = static_cast<float>(std::abs(cz - gz));
            return (dx + dz) + (1.4142135f - 2.0f) * std::min(dx, dz);
        };

        gScore[startIdx] = 0.0f;
        open.push({ heuristic(sx, sz), startIdx });

        static const int kDx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
        static const int kDz[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
        static const float kCost[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.4142135f, 1.4142135f, 1.4142135f, 1.4142135f };

        bool found = false;
        while (!open.empty())
        {
            Node cur = open.top();
            open.pop();
            if (closed[cur.idx]) continue;
            closed[cur.idx] = 1;
            if (cur.idx == goalIdx)
            {
                found = true;
                break;
            }

            int cx = cur.idx % gridN;
            int cz = cur.idx / gridN;

            for (int d = 0; d < 8; ++d)
            {
                int nx = cx + kDx[d];
                int nz = cz + kDz[d];
                if (nx < 0 || nx >= gridN || nz < 0 || nz >= gridN)
                    continue;
                int nIdx = nz * gridN + nx;
                if (blocked[nIdx] || closed[nIdx])
                    continue;
                // Prevent squeezing diagonally between two blocked corner cells
                if (d >= 4 && (blocked[cz * gridN + nx] || blocked[nz * gridN + cx]))
                    continue;

                float candG = gScore[cur.idx] + kCost[d];
                if (candG < gScore[nIdx])
                {
                    gScore[nIdx] = candG;
                    cameFrom[nIdx] = cur.idx;
                    open.push({ candG + heuristic(nx, nz), nIdx });
                }
            }
        }

        if (!found)
            return { glm::vec3(goal.x, start.y, goal.z) };

        std::vector<int> rev;
        for (int c = goalIdx; c != -1 && c != startIdx; c = cameFrom[c])
            rev.push_back(c);
        std::reverse(rev.begin(), rev.end());

        std::vector<glm::vec3> rawPts;
        rawPts.reserve(rev.size());
        for (size_t i = 0; i < rev.size(); ++i)
        {
            int idx = rev[i];
            glm::vec3 pt = (idx == goalIdx) ? glm::vec3(goal.x, start.y, goal.z) : cellToWorld(idx % gridN, idx / gridN);
            if (rawPts.size() >= 2)
            {
                glm::vec2 d0 = glm::vec2(rawPts.back().x - rawPts[rawPts.size() - 2].x, rawPts.back().z - rawPts[rawPts.size() - 2].z);
                glm::vec2 d1 = glm::vec2(pt.x - rawPts.back().x, pt.z - rawPts.back().z);
                if (glm::length(d0) > 0.001f && glm::length(d1) > 0.001f &&
                    glm::dot(glm::normalize(d0), glm::normalize(d1)) > 0.995f)
                {
                    rawPts.back() = pt;
                    continue;
                }
            }
            rawPts.push_back(pt);
        }
        return rawPts;
    }

    void PhysicsSystem::Step(Scene& scene, float dt)
    {
        m_Collisions.clear();
        m_TriggerEntered.clear();
        m_TriggerExited.clear();
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

                if (isTrigger)
                {
                    ApplyArea3DOverride(a, b, dt);
                    ApplyArea3DOverride(b, a, dt);
                }
                else
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

        // 3. Track Trigger Enter / Exit events (Godot Area3D body_entered / body_exited)
        std::set<std::pair<EntityID, EntityID>> currTriggers;
        for (const auto& col : m_Collisions)
        {
            if (!col.isTrigger) continue;
            EntityID lo = std::min(col.entityA, col.entityB);
            EntityID hi = std::max(col.entityA, col.entityB);
            auto key = std::make_pair(lo, hi);
            currTriggers.insert(key);
            if (m_PrevTriggers.find(key) == m_PrevTriggers.end())
                m_TriggerEntered.push_back(col);
        }
        for (const auto& prev : m_PrevTriggers)
        {
            if (currTriggers.find(prev) == currTriggers.end())
            {
                CollisionEvent exited;
                exited.entityA = prev.first;
                exited.entityB = prev.second;
                exited.isTrigger = true;
                m_TriggerExited.push_back(exited);
            }
        }
        m_PrevTriggers = std::move(currTriggers);

        // 4. Continuously update Godot RayCast3D nodes
        UpdateRayCastNodes(scene);
    }
}
