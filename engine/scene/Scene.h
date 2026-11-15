#pragma once
#include "Components.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

namespace Hargon
{
    class Scene
    {
    public:
        using SignalCallback = std::function<void(EntityID emitterId, const std::string& payload)>;

        Scene() = default;
        ~Scene() = default;

        void Clear();

        Entity& CreateEntity(const std::string& name = "Entity", const std::string& tag = "Default");
        EntityID DuplicateEntity(EntityID sourceId);
        void DestroyEntity(EntityID id);
        void FlushDestroyedEntities();

        Entity* GetEntity(EntityID id);
        const Entity* GetEntity(EntityID id) const;
        Entity* FindEntityByName(const std::string& name);
        std::vector<EntityID> FindEntitiesByTag(const std::string& tag) const;

        // Godot-style Node3D Hierarchy (Parent / Children & Global Transform computation)
        bool SetParent(EntityID childId, EntityID newParentId);
        glm::mat4 GetWorldMatrix(const Entity& e) const;
        glm::vec3 GetWorldPosition(const Entity& e) const;

        // Godot-style Node Groups & Signal Bus
        std::vector<EntityID> GetEntitiesInGroup(const std::string& group) const;
        void ConnectSignal(const std::string& signalName, SignalCallback cb);
        void EmitSignal(const std::string& signalName, EntityID emitterId = kInvalidEntity, const std::string& payload = "");
        const std::vector<std::string>& GetRecentSignals() const { return m_RecentSignals; }

        std::vector<Entity>& GetEntities() { return m_Entities; }
        const std::vector<Entity>& GetEntities() const { return m_Entities; }

        EntityID Raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float* outDist = nullptr) const;
        bool ShouldProcessEntity(const Entity& e, bool isPaused) const;

    public:
        std::string sceneName = "Untitled 3D Scene";

        // Environment settings
        glm::vec3 skyZenithColor{ 0.03f, 0.06f, 0.16f };
        glm::vec3 skyHorizonColor{ 0.14f, 0.34f, 0.56f };
        glm::vec3 groundAmbientColor{ 0.08f, 0.10f, 0.15f };
        float ambientIntensity = 0.55f;
        float fogDensity = 0.0045f;
        float arenaHalfSize = 36.0f;
        WorldEnvironmentSettings worldEnv;

        // Live Gameplay state
        int score = 0;
        int wave = 1;
        int maxWaves = 3;
        int enemiesRemainingInWave = 0;
        int enemiesKilled = 0;
        int crystalsCollected = 0;
        float waveCooldownTimer = 0.0f;
        bool gameOver = false;
        bool gameWon = false;
        std::string hudBannerText;
        float hudBannerTimer = 4.0f;

        // Data-driven HUD vocabulary (serialized per scene, so the engine never hard-codes game names)
        std::string introBannerText = "ENTERING SCENE";
        std::string hudHealthLabel = "HEALTH";
        std::string hudShieldLabel = "SHIELD";
        std::string hudObjectiveLabel = "OBJECTIVE";
        std::string hudCollectibleLabel = "PICKUPS";
        std::string hudControlsText = "[W/S] Move   [A/D] Turn   [SPACE] Fire   [SHIFT] Boost   [R] Restart";
        std::string victoryText = "VICTORY!";
        std::string defeatText = "MISSION FAILED! PRESS [R] TO RESTART";

        // Set by gameplay when the player asks to restart; the host (editor/runtime) reloads the startup scene.
        bool restartRequested = false;

    private:
        std::vector<Entity> m_Entities;
        EntityID m_NextEntityID = 1;
        std::unordered_map<std::string, std::vector<SignalCallback>> m_SignalListeners;
        std::vector<std::string> m_RecentSignals;
    };
}
