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

    static bool RayIntersectAABB(const glm::vec3& orig, const glm::vec3& dir, const glm::vec3& bmin, const glm::vec3& bmax, float& tHit)
    {
        float tmin = -std::numeric_limits<float>::infinity();
        float tmax =  std::numeric_limits<float>::infinity();

        for (int a = 0; a < 3; ++a)
        {
            if (std::abs(dir[a]) < 1e-6f)
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

    EntityID Scene::Raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float* outDist) const
    {
        float bestDist = std::numeric_limits<float>::max();
        EntityID bestEntity = kInvalidEntity;

        for (const auto& e : m_Entities)
        {
            if (!e.alive || !e.mesh.enabled)
                continue;

            // Skip large ground plane unless nothing else is hit
            bool isGround = (e.tag.tag == "Environment" && std::max(e.transform.scale.x, e.transform.scale.z) > 30.0f);

            glm::vec3 halfExt = e.transform.scale * 0.55f;
            halfExt = glm::max(halfExt, glm::vec3(0.25f));
            glm::vec3 bmin = e.transform.position - halfExt;
            glm::vec3 bmax = e.transform.position + halfExt;

            float t = 0.0f;
            if (RayIntersectAABB(rayOrigin, rayDir, bmin, bmax, t))
            {
                if (isGround)
                    t += 500.0f; // deprioritize floor so objects on it are easy to pick

                if (t < bestDist)
                {
                    bestDist = t;
                    bestEntity = e.id;
                }
            }
        }

        if (outDist && bestEntity != kInvalidEntity)
            *outDist = bestDist;
        return bestEntity;
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
