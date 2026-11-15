#include "Scene.h"
#include "../core/Log.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Hargon
{
    void Scene::Clear()
    {
        m_Entities.clear();
        m_Entities.reserve(512);
        m_NextEntityID = 1;
        m_SignalListeners.clear();
        m_RecentSignals.clear();

        sceneName = "Untitled 3D Scene";
        skyZenithColor = glm::vec3(0.03f, 0.06f, 0.16f);
        skyHorizonColor = glm::vec3(0.14f, 0.34f, 0.56f);
        groundAmbientColor = glm::vec3(0.08f, 0.10f, 0.15f);
        ambientIntensity = 0.55f;
        fogDensity = 0.0045f;
        arenaHalfSize = 36.0f;
        worldEnv = WorldEnvironmentSettings{};

        score = 0;
        wave = 1;
        maxWaves = 3;
        enemiesRemainingInWave = 0;
        enemiesKilled = 0;
        crystalsCollected = 0;
        waveCooldownTimer = 0.0f;
        gameOver = false;
        gameWon = false;
        restartRequested = false;

        introBannerText = "ENTERING SCENE";
        hudHealthLabel = "HEALTH";
        hudShieldLabel = "SHIELD";
        hudObjectiveLabel = "OBJECTIVE";
        hudCollectibleLabel = "PICKUPS";
        hudControlsText = "[W/S] Move   [A/D] Turn   [SPACE] Fire   [SHIFT] Boost   [R] Restart";
        victoryText = "VICTORY!";
        defeatText = "MISSION FAILED! PRESS [R] TO RESTART";
        hudBannerText = introBannerText;
        hudBannerTimer = 3.5f;
    }

    Entity& Scene::CreateEntity(const std::string& name, const std::string& tag)
    {
        if (m_Entities.capacity() < 512)
            m_Entities.reserve(512);

        Entity e;
        e.id = m_NextEntityID++;
        e.alive = true;
        e.tag.name = name;
        e.tag.tag = tag;
        if (!tag.empty() && tag != "Default")
            e.tag.AddToGroup(tag);
        m_Entities.push_back(e);
        return m_Entities.back();
    }

    EntityID Scene::DuplicateEntity(EntityID sourceId)
    {
        const Entity* src = GetEntity(sourceId);
        if (!src) return kInvalidEntity;

        Entity copy = *src;
        copy.id = m_NextEntityID++;
        copy.tag.name = src->tag.name + "_Copy";
        copy.transform.position += glm::vec3(1.5f, 0.0f, 1.5f);
        copy.luaScript.initialized = false;
        copy.childrenIds.clear();
        m_Entities.push_back(copy);
        if (copy.parentId != kInvalidEntity)
        {
            if (Entity* p = GetEntity(copy.parentId))
                p->childrenIds.push_back(copy.id);
        }
        return copy.id;
    }

    void Scene::DestroyEntity(EntityID id)
    {
        for (auto& e : m_Entities)
        {
            if (e.id == id)
            {
                if (e.parentId != kInvalidEntity)
                {
                    if (Entity* p = GetEntity(e.parentId))
                    {
                        auto& c = p->childrenIds;
                        c.erase(std::remove(c.begin(), c.end(), id), c.end());
                    }
                }
                for (EntityID childId : e.childrenIds)
                {
                    if (Entity* ch = GetEntity(childId))
                        ch->parentId = kInvalidEntity;
                }
                e.childrenIds.clear();
                e.alive = false;
                break;
            }
        }
    }

    void Scene::FlushDestroyedEntities()
    {
        m_Entities.erase(
            std::remove_if(m_Entities.begin(), m_Entities.end(), [](const Entity& e) { return !e.alive; }),
            m_Entities.end());
    }

    Entity* Scene::GetEntity(EntityID id)
    {
        if (id == kInvalidEntity) return nullptr;
        for (auto& e : m_Entities)
        {
            if (e.alive && e.id == id)
                return &e;
        }
        return nullptr;
    }

    const Entity* Scene::GetEntity(EntityID id) const
    {
        if (id == kInvalidEntity) return nullptr;
        for (const auto& e : m_Entities)
        {
            if (e.alive && e.id == id)
                return &e;
        }
        return nullptr;
    }

    Entity* Scene::FindEntityByName(const std::string& name)
    {
        for (auto& e : m_Entities)
        {
            if (e.alive && e.tag.name == name)
                return &e;
        }
        return nullptr;
    }

    std::vector<EntityID> Scene::FindEntitiesByTag(const std::string& tag) const
    {
        std::vector<EntityID> result;
        for (const auto& e : m_Entities)
        {
            if (e.alive && e.tag.tag == tag)
                result.push_back(e.id);
        }
        return result;
    }

    bool Scene::SetParent(EntityID childId, EntityID newParentId)
    {
        if (childId == kInvalidEntity || childId == newParentId)
            return false;

        Entity* child = GetEntity(childId);
        if (!child)
            return false;

        // Check for cycle in parent chain
        EntityID cursor = newParentId;
        int guard = 0;
        while (cursor != kInvalidEntity && guard < 64)
        {
            if (cursor == childId)
                return false;
            const Entity* p = GetEntity(cursor);
            cursor = p ? p->parentId : kInvalidEntity;
            ++guard;
        }

        // Detach from old parent
        if (child->parentId != kInvalidEntity)
        {
            if (Entity* oldP = GetEntity(child->parentId))
            {
                auto& ch = oldP->childrenIds;
                ch.erase(std::remove(ch.begin(), ch.end(), childId), ch.end());
            }
        }

        child->parentId = newParentId;
        if (newParentId != kInvalidEntity)
        {
            if (Entity* newP = GetEntity(newParentId))
            {
                if (std::find(newP->childrenIds.begin(), newP->childrenIds.end(), childId) == newP->childrenIds.end())
                    newP->childrenIds.push_back(childId);
            }
        }
        return true;
    }

    glm::mat4 Scene::GetWorldMatrix(const Entity& e) const
    {
        glm::mat4 mat = e.transform.GetTransformMatrix();
        EntityID pId = e.parentId;
        int depth = 0;
        while (pId != kInvalidEntity && depth < 16)
        {
            const Entity* parent = GetEntity(pId);
            if (!parent)
                break;
            mat = parent->transform.GetTransformMatrix() * mat;
            pId = parent->parentId;
            ++depth;
        }
        return mat;
    }

    glm::vec3 Scene::GetWorldPosition(const Entity& e) const
    {
        if (e.parentId == kInvalidEntity)
            return e.transform.position;
        glm::mat4 wm = GetWorldMatrix(e);
        return glm::vec3(wm[3]);
    }

    std::vector<EntityID> Scene::GetEntitiesInGroup(const std::string& group) const
    {
        std::vector<EntityID> result;
        for (const auto& e : m_Entities)
        {
            if (e.alive && (e.tag.IsInGroup(group) || e.tag.tag == group))
                result.push_back(e.id);
        }
        return result;
    }

    void Scene::ConnectSignal(const std::string& signalName, SignalCallback cb)
    {
        if (!signalName.empty() && cb)
            m_SignalListeners[signalName].push_back(std::move(cb));
    }

    void Scene::EmitSignal(const std::string& signalName, EntityID emitterId, const std::string& payload)
    {
        std::string entry = "[Signal] " + signalName + " (from Entity " + std::to_string(emitterId) + ")" +
                            (payload.empty() ? "" : (" : " + payload));
        m_RecentSignals.push_back(entry);
        if (m_RecentSignals.size() > 32)
            m_RecentSignals.erase(m_RecentSignals.begin());

        auto it = m_SignalListeners.find(signalName);
        if (it != m_SignalListeners.end())
        {
            auto listenersCopy = it->second;
            for (auto& cb : listenersCopy)
            {
                if (cb) cb(emitterId, payload);
            }
        }
    }

    static glm::vec3 ExtractEulerYXZDegrees(const glm::mat3& rotMat)
    {
        float sx = glm::clamp(-rotMat[2][1], -1.0f, 1.0f);
        float rx = std::asin(sx);
        float ry = 0.0f;
        float rz = 0.0f;

        if (std::abs(std::cos(rx)) > 1e-4f)
        {
            ry = std::atan2(rotMat[2][0], rotMat[2][2]);
            rz = std::atan2(rotMat[0][1], rotMat[1][1]);
        }
        else
        {
            ry = std::atan2(-rotMat[0][2], rotMat[0][0]);
            rz = 0.0f;
        }
        return glm::degrees(glm::vec3(rx, ry, rz));
    }

    bool Scene::CanReparent(EntityID childId, EntityID newParentId) const
    {
        if (childId == kInvalidEntity || !GetEntity(childId))
            return false;
        if (newParentId == kInvalidEntity)
            return true;
        if (childId == newParentId || !GetEntity(newParentId))
            return false;

        EntityID cur = newParentId;
        size_t guard = 0;
        while (cur != kInvalidEntity && guard < m_Entities.size() + 4)
        {
            if (cur == childId)
                return false;
            const Entity* e = GetEntity(cur);
            cur = e ? e->parentId : kInvalidEntity;
            ++guard;
        }
        return true;
    }

    bool Scene::SetParentKeepWorld(EntityID childId, EntityID newParentId)
    {
        if (!CanReparent(childId, newParentId))
            return false;

        Entity* child = GetEntity(childId);
        if (!child)
            return false;
        if (child->parentId == newParentId)
            return true;

        glm::mat4 worldMat = GetWorldMatrix(*child);
        if (!SetParent(childId, newParentId))
            return false;

        child = GetEntity(childId);
        glm::mat4 parentWorld(1.0f);
        if (newParentId != kInvalidEntity)
        {
            if (const Entity* p = GetEntity(newParentId))
                parentWorld = GetWorldMatrix(*p);
        }

        glm::mat4 localMat = (std::abs(glm::determinant(parentWorld)) > 1e-10f)
            ? (glm::inverse(parentWorld) * worldMat)
            : worldMat;

        child->transform.position = glm::vec3(localMat[3]);

        glm::vec3 sx = glm::vec3(localMat[0]);
        glm::vec3 sy = glm::vec3(localMat[1]);
        glm::vec3 sz = glm::vec3(localMat[2]);
        float lx = glm::length(sx);
        float ly = glm::length(sy);
        float lz = glm::length(sz);
        if (lx < 1e-6f) { sx = glm::vec3(1, 0, 0); lx = 1e-6f; } else sx /= lx;
        if (ly < 1e-6f) { sy = glm::vec3(0, 1, 0); ly = 1e-6f; } else sy /= ly;
        if (lz < 1e-6f) { sz = glm::vec3(0, 0, 1); lz = 1e-6f; } else sz /= lz;

        glm::mat3 rotMat(sx, sy, sz);
        if (glm::determinant(rotMat) < 0.0f)
        {
            lx = -lx;
            rotMat[0] = -rotMat[0];
        }

        child->transform.scale = glm::vec3(lx, ly, lz);
        child->transform.rotation = ExtractEulerYXZDegrees(rotMat);
        return true;
    }

    bool Scene::ReorderEntity(EntityID sourceId, EntityID targetId, bool insertAfter)
    {
        if (sourceId == kInvalidEntity || targetId == kInvalidEntity || sourceId == targetId)
            return false;

        int srcIdx = -1;
        int dstIdx = -1;
        for (int i = 0; i < static_cast<int>(m_Entities.size()); ++i)
        {
            if (m_Entities[i].id == sourceId) srcIdx = i;
            if (m_Entities[i].id == targetId) dstIdx = i;
        }
        if (srcIdx < 0 || dstIdx < 0)
            return false;

        Entity moved = m_Entities[srcIdx];
        EntityID movedParent = moved.parentId;
        m_Entities.erase(m_Entities.begin() + srcIdx);
        if (srcIdx < dstIdx)
            --dstIdx;

        int insertIdx = dstIdx + (insertAfter ? 1 : 0);
        insertIdx = std::clamp(insertIdx, 0, static_cast<int>(m_Entities.size()));
        m_Entities.insert(m_Entities.begin() + insertIdx, std::move(moved));

        if (movedParent != kInvalidEntity)
        {
            if (Entity* parent = GetEntity(movedParent))
            {
                parent->childrenIds.clear();
                for (const auto& e : m_Entities)
                {
                    if (e.alive && e.parentId == movedParent)
                        parent->childrenIds.push_back(e.id);
                }
            }
        }
        return true;
    }

    void Scene::CollectSubtree(EntityID rootId, std::vector<EntityID>& outIds) const
    {
        outIds.clear();
        if (!GetEntity(rootId))
            return;

        std::function<void(EntityID)> dfs = [&](EntityID cur) {
            if (std::find(outIds.begin(), outIds.end(), cur) != outIds.end())
                return;
            outIds.push_back(cur);
            for (const auto& e : m_Entities)
            {
                if (e.alive && e.parentId == cur)
                    dfs(e.id);
            }
        };
        dfs(rootId);
    }

    void Scene::DestroySubtree(EntityID rootId)
    {
        std::vector<EntityID> ids;
        CollectSubtree(rootId, ids);
        for (EntityID id : ids)
            DestroyEntity(id);
        FlushDestroyedEntities();
    }

    EntityID Scene::DuplicateSubtree(EntityID sourceId, const glm::vec3& offset)
    {
        std::vector<EntityID> ids;
        CollectSubtree(sourceId, ids);
        if (ids.empty())
            return kInvalidEntity;

        const Entity* srcRoot = GetEntity(sourceId);
        EntityID origRootParent = srcRoot ? srcRoot->parentId : kInvalidEntity;

        std::vector<Entity> snapshots;
        snapshots.reserve(ids.size());
        for (EntityID id : ids)
        {
            if (const Entity* e = GetEntity(id))
                snapshots.push_back(*e);
        }

        std::unordered_map<EntityID, EntityID> oldToNew;
        for (const Entity& snap : snapshots)
        {
            Entity& dup = CreateEntity(snap.tag.name, snap.tag.tag);
            EntityID assignedId = dup.id;
            dup = snap;
            dup.id = assignedId;
            dup.alive = true;
            dup.parentId = kInvalidEntity;
            dup.childrenIds.clear();
            oldToNew[snap.id] = assignedId;
        }

        for (const Entity& snap : snapshots)
        {
            EntityID newId = oldToNew[snap.id];
            Entity* dup = GetEntity(newId);
            if (!dup) continue;
            if (snap.id == sourceId)
            {
                dup->tag.name += " Copy";
                dup->transform.position += offset;
                SetParent(newId, origRootParent);
            }
            else
            {
                auto it = oldToNew.find(snap.parentId);
                EntityID newParent = (it != oldToNew.end()) ? it->second : kInvalidEntity;
                SetParent(newId, newParent);
            }
        }

        return oldToNew[sourceId];
    }

    EntityID Scene::GroupEntities(const std::vector<EntityID>& entityIds, const std::string& groupName)
    {
        std::vector<EntityID> validTopLevel;
        for (EntityID id : entityIds)
        {
            const Entity* e = GetEntity(id);
            if (!e || !e->alive) continue;

            bool ancestorSelected = false;
            EntityID cur = e->parentId;
            size_t guard = 0;
            while (cur != kInvalidEntity && guard < m_Entities.size() + 4)
            {
                if (std::find(entityIds.begin(), entityIds.end(), cur) != entityIds.end())
                {
                    ancestorSelected = true;
                    break;
                }
                const Entity* p = GetEntity(cur);
                cur = p ? p->parentId : kInvalidEntity;
                ++guard;
            }
            if (!ancestorSelected && std::find(validTopLevel.begin(), validTopLevel.end(), id) == validTopLevel.end())
                validTopLevel.push_back(id);
        }

        if (validTopLevel.empty())
            return kInvalidEntity;

        glm::vec3 centroid(0.0f);
        EntityID commonParent = GetEntity(validTopLevel[0])->parentId;
        for (EntityID id : validTopLevel)
        {
            const Entity* e = GetEntity(id);
            centroid += GetWorldPosition(*e);
            if (e->parentId != commonParent)
                commonParent = kInvalidEntity;
        }
        centroid /= static_cast<float>(validTopLevel.size());

        Entity& group = CreateEntity(groupName.empty() ? "Group" : groupName, "Group");
        group.mesh.enabled = false;
        group.hasMesh = false;
        EntityID groupId = group.id;
        if (commonParent != kInvalidEntity)
        {
            if (const Entity* p = GetEntity(commonParent))
            {
                glm::mat4 pWorld = GetWorldMatrix(*p);
                if (std::abs(glm::determinant(pWorld)) > 1e-10f)
                    group.transform.position = glm::vec3(glm::inverse(pWorld) * glm::vec4(centroid, 1.0f));
                else
                    group.transform.position = centroid;
            }
            SetParent(groupId, commonParent);
        }
        else
        {
            group.transform.position = centroid;
        }

        for (EntityID id : validTopLevel)
            SetParentKeepWorld(id, groupId);

        return groupId;
    }

    glm::mat3 Scene::GetWorldRotationBasis(const Entity& e) const
    {
        glm::mat4 world = GetWorldMatrix(e);
        glm::vec3 x = glm::vec3(world[0]);
        glm::vec3 y = glm::vec3(world[1]);
        glm::vec3 z = glm::vec3(world[2]);
        x = (glm::length(x) > 1e-6f) ? glm::normalize(x) : glm::vec3(1, 0, 0);
        y = (glm::length(y) > 1e-6f) ? glm::normalize(y) : glm::vec3(0, 1, 0);
        z = (glm::length(z) > 1e-6f) ? glm::normalize(z) : glm::vec3(0, 0, 1);
        return glm::mat3(x, y, z);
    }

    static bool RayIntersectAABB(const glm::vec3& orig, const glm::vec3& dir, const glm::vec3& bmin, const glm::vec3& bmax, float& tHit)
    {
        float tmin = -std::numeric_limits<float>::infinity();
        float tmax =  std::numeric_limits<float>::infinity();

        for (int a = 0; a < 3; ++a)
        {
            if (std::abs(dir[a]) < 1e-8f)
            {
                if (orig[a] < bmin[a] || orig[a] > bmax[a])
                    return false;
            }
            else
            {
                float invD = 1.0f / dir[a];
                float t0 = (bmin[a] - orig[a]) * invD;
                float t1 = (bmax[a] - orig[a]) * invD;
                if (t0 > t1) std::swap(t0, t1);
                tmin = std::max(tmin, t0);
                tmax = std::min(tmax, t1);
                if (tmax < tmin) return false;
            }
        }

        if (tmax < 0.0f) return false;
        tHit = (tmin >= 0.0f) ? tmin : tmax;
        return true;
    }

    static bool RayIntersectSphere(const glm::vec3& orig, const glm::vec3& unitDir, const glm::vec3& center, float radius, float& tHit)
    {
        glm::vec3 oc = orig - center;
        float b = glm::dot(oc, unitDir);
        float c = glm::dot(oc, oc) - radius * radius;
        float disc = b * b - c;
        if (disc < 0.0f) return false;
        float sq = std::sqrt(disc);
        float t = -b - sq;
        if (t < 0.0f) t = -b + sq;
        if (t < 0.0f) return false;
        tHit = t;
        return true;
    }

    EntityID Scene::Raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float* outDist, const BoundsLookupFn& boundsLookup) const
    {
        float dirLen = glm::length(rayDir);
        if (dirLen < 1e-8f) return kInvalidEntity;
        glm::vec3 unitDir = rayDir / dirLen;

        float bestDist = std::numeric_limits<float>::max();
        EntityID bestEntity = kInvalidEntity;

        for (const auto& e : m_Entities)
        {
            if (!e.alive)
                continue;

            if (e.mesh.enabled)
            {
                glm::vec3 bmin(-0.5f), bmax(0.5f);
                if (boundsLookup)
                {
                    glm::vec3 qmin, qmax;
                    if (boundsLookup(e.mesh, qmin, qmax))
                    {
                        bmin = qmin;
                        bmax = qmax;
                    }
                }
                for (int a = 0; a < 3; ++a)
                {
                    if (bmax[a] - bmin[a] < 0.02f)
                    {
                        bmin[a] -= 0.01f;
                        bmax[a] += 0.01f;
                    }
                }

                glm::mat4 world = GetWorldMatrix(e);
                if (std::abs(glm::determinant(world)) <= 1e-10f)
                    continue;
                glm::mat4 invWorld = glm::inverse(world);
                glm::vec3 localOrig = glm::vec3(invWorld * glm::vec4(rayOrigin, 1.0f));
                glm::vec3 localDir  = glm::vec3(invWorld * glm::vec4(unitDir, 0.0f));

                float t = 0.0f;
                if (RayIntersectAABB(localOrig, localDir, bmin, bmax, t))
                {
                    if (t >= 0.0f && t < bestDist)
                    {
                        bestDist = t;
                        bestEntity = e.id;
                    }
                }
            }
            else if (e.light.enabled || e.camera.enabled || e.particleEmitter.enabled)
            {
                glm::vec3 worldPos = GetWorldPosition(e);
                float t = 0.0f;
                if (RayIntersectSphere(rayOrigin, unitDir, worldPos, 0.45f, t))
                {
                    if (t >= 0.0f && t < bestDist)
                    {
                        bestDist = t;
                        bestEntity = e.id;
                    }
                }
            }
        }

        if (outDist && bestEntity != kInvalidEntity)
            *outDist = bestDist;
        return bestEntity;
    }

    bool Scene::RaycastSurface(const glm::vec3& rayOrigin, const glm::vec3& rayDir, EntityID ignoreEntity, float& outHitDist, glm::vec3& outHitPos, const BoundsLookupFn& boundsLookup) const
    {
        float dirLen = glm::length(rayDir);
        if (dirLen < 1e-8f) return false;
        glm::vec3 unitDir = rayDir / dirLen;

        std::vector<EntityID> ignored;
        if (ignoreEntity != kInvalidEntity)
            CollectSubtree(ignoreEntity, ignored);

        float bestDist = std::numeric_limits<float>::max();
        bool found = false;

        for (const auto& e : m_Entities)
        {
            if (!e.alive || !e.mesh.enabled)
                continue;
            if (std::find(ignored.begin(), ignored.end(), e.id) != ignored.end())
                continue;

            glm::vec3 bmin(-0.5f), bmax(0.5f);
            if (boundsLookup)
            {
                glm::vec3 qmin, qmax;
                if (boundsLookup(e.mesh, qmin, qmax))
                {
                    bmin = qmin;
                    bmax = qmax;
                }
            }
            for (int a = 0; a < 3; ++a)
            {
                if (bmax[a] - bmin[a] < 0.02f)
                {
                    bmin[a] -= 0.01f;
                    bmax[a] += 0.01f;
                }
            }

            glm::mat4 world = GetWorldMatrix(e);
            if (std::abs(glm::determinant(world)) <= 1e-10f)
                continue;
            glm::mat4 invWorld = glm::inverse(world);
            glm::vec3 localOrig = glm::vec3(invWorld * glm::vec4(rayOrigin, 1.0f));
            glm::vec3 localDir  = glm::vec3(invWorld * glm::vec4(unitDir, 0.0f));

            float t = 0.0f;
            if (RayIntersectAABB(localOrig, localDir, bmin, bmax, t))
            {
                if (t >= 0.0f && t < bestDist)
                {
                    bestDist = t;
                    found = true;
                }
            }
        }

        if (!found) return false;
        outHitDist = bestDist;
        outHitPos = rayOrigin + unitDir * bestDist;
        return true;
    }

    bool Scene::DropEntityToSurface(EntityID entityId, const BoundsLookupFn& boundsLookup)
    {
        Entity* e = GetEntity(entityId);
        if (!e || !e->alive) return false;

        glm::mat4 world = GetWorldMatrix(*e);
        glm::vec3 worldPos = glm::vec3(world[3]);

        float minWorldY = worldPos.y;
        if (e->mesh.enabled)
        {
            glm::vec3 bmin(-0.5f), bmax(0.5f);
            if (boundsLookup)
            {
                glm::vec3 qmin, qmax;
                if (boundsLookup(e->mesh, qmin, qmax))
                {
                    bmin = qmin;
                    bmax = qmax;
                }
            }
            minWorldY = std::numeric_limits<float>::max();
            for (int cx = 0; cx < 2; ++cx)
            for (int cy = 0; cy < 2; ++cy)
            for (int cz = 0; cz < 2; ++cz)
            {
                glm::vec3 corner(
                    cx ? bmax.x : bmin.x,
                    cy ? bmax.y : bmin.y,
                    cz ? bmax.z : bmin.z);
                glm::vec3 wc = glm::vec3(world * glm::vec4(corner, 1.0f));
                minWorldY = std::min(minWorldY, wc.y);
            }
        }

        glm::vec3 rayOrigin(worldPos.x, minWorldY + 0.35f, worldPos.z);
        float hitDist = 0.0f;
        glm::vec3 hitPos(0.0f);
        if (!RaycastSurface(rayOrigin, glm::vec3(0.0f, -1.0f, 0.0f), entityId, hitDist, hitPos, boundsLookup))
            return false;

        float deltaY = hitPos.y - minWorldY;
        glm::vec3 targetWorldPos = worldPos + glm::vec3(0.0f, deltaY, 0.0f);

        if (e->parentId != kInvalidEntity)
        {
            if (const Entity* p = GetEntity(e->parentId))
            {
                glm::mat4 pWorld = GetWorldMatrix(*p);
                if (std::abs(glm::determinant(pWorld)) > 1e-10f)
                {
                    e->transform.position = glm::vec3(glm::inverse(pWorld) * glm::vec4(targetWorldPos, 1.0f));
                    return true;
                }
            }
        }
        e->transform.position = targetWorldPos;
        return true;
    }

    bool Scene::ShouldProcessEntity(const Entity& e, bool isPaused) const
    {
        if (!e.alive)
            return false;

        switch (e.processMode)
        {
        case ProcessMode::Disabled:
            return false;
        case ProcessMode::Always:
            return true;
        case ProcessMode::WhenPaused:
            return isPaused;
        case ProcessMode::Pausable:
            return !isPaused;
        case ProcessMode::Inherit:
        default:
            if (e.parentId != kInvalidEntity)
            {
                const Entity* parent = GetEntity(e.parentId);
                if (parent)
                    return ShouldProcessEntity(*parent, isPaused);
            }
            return !isPaused;
        }
    }
}
