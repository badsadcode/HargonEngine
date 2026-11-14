#pragma once
#include "../renderer/Mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace Hargon
{
    using EntityID = uint32_t;
    constexpr EntityID kInvalidEntity = 0;

    // Inspired by Godot Engine's Node::ProcessMode (scene/main/node.h)
    enum class ProcessMode
    {
        Inherit = 0,
        Pausable = 1,
        WhenPaused = 2,
        Always = 3,
        Disabled = 4
    };

    // Inspired by Godot Engine's Environment::ToneMapper (scene/resources/environment.h)
    enum class ToneMapperMode
    {
        Linear = 0,
        Reinhard = 1,
        Filmic = 2,
        ACES = 3,
        AgX = 4
    };

    struct WorldEnvironmentSettings
    {
        ToneMapperMode tonemapMode = ToneMapperMode::AgX;
        float tonemapExposure = 1.05f;
        float tonemapWhite = 1.0f;

        bool glowEnabled = true;
        float glowIntensity = 0.35f;
        float glowBloom = 0.15f;

        float fogHeight = 0.0f;
        float fogHeightDensity = 0.0f;
        float fogSunScatter = 0.22f;

        bool adjustmentEnabled = true;
        float adjustmentBrightness = 1.0f;
        float adjustmentContrast = 1.04f;
        float adjustmentSaturation = 1.08f;

        bool frustumCullingEnabled = true;
    };

    struct TagComponent
    {
        std::string name = "Entity";
        std::string tag = "Default";
        std::vector<std::string> groups; // Godot-style multi-group membership

        void AddToGroup(const std::string& g)
        {
            if (!g.empty() && !IsInGroup(g))
                groups.push_back(g);
        }

        void RemoveFromGroup(const std::string& g)
        {
            groups.erase(std::remove(groups.begin(), groups.end(), g), groups.end());
        }

        bool IsInGroup(const std::string& g) const
        {
            return std::find(groups.begin(), groups.end(), g) != groups.end();
        }
    };

    struct TransformComponent
    {
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        glm::vec3 rotation{ 0.0f, 0.0f, 0.0f }; // Euler angles in degrees (Pitch X, Yaw Y, Roll Z)
        glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

        glm::mat4 GetRotationMatrix() const
        {
            glm::vec3 rad = glm::radians(rotation);
            return glm::yawPitchRoll(rad.y, rad.x, rad.z);
        }

        glm::mat4 GetTransformMatrix() const
        {
            glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 R = GetRotationMatrix();
            glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
            return T * R * S;
        }

        glm::vec3 GetForward() const
        {
            glm::vec4 f = GetRotationMatrix() * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
            return glm::normalize(glm::vec3(f));
        }

        glm::vec3 GetRight() const
        {
            glm::vec4 r = GetRotationMatrix() * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
            return glm::normalize(glm::vec3(r));
        }

        glm::vec3 GetUp() const
        {
            glm::vec4 u = GetRotationMatrix() * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
            return glm::normalize(glm::vec3(u));
        }
    };

    struct MeshComponent
    {
        bool enabled = true;
        MeshPrimitive primitive = MeshPrimitive::Cube;
        std::string customObjPath;
    };

    struct MaterialComponent
    {
        glm::vec4 albedoColor{ 0.85f, 0.85f, 0.90f, 1.0f };
        float metallic = 0.25f;
        float roughness = 0.40f;
        glm::vec3 emissiveColor{ 0.0f, 0.0f, 0.0f };
        float emissiveStrength = 0.0f;
        float uvScale = 1.0f;
        int patternType = 0; // 0=Solid/Tex, 1=ArenaGrid, 2=Checker, 3=TechPanels, 4=CrystalFacet
    };

    struct LightComponent
    {
        bool enabled = false;
        bool isDirectional = false;
        glm::vec3 color{ 1.0f, 0.95f, 0.85f };
        float intensity = 2.5f;
        float range = 18.0f;
    };

    struct CameraComponent
    {
        bool enabled = false;
        bool isPrimary = false;
        float fov = 55.0f;
        float nearClip = 0.1f;
        float farClip = 350.0f;
        std::string followTargetName = "PlayerRover";
        glm::vec3 followOffset{ 0.0f, 14.0f, -15.5f };
        glm::vec3 followLookOffset{ 0.0f, 0.6f, 3.5f };
        float smoothSpeed = 7.5f;
    };

    struct RigidBodyComponent
    {
        bool enabled = false;
        bool isKinematic = false;
        bool useGravity = false;
        bool freezeY = true;
        float mass = 1.0f;
        float drag = 4.0f;
        float bounciness = 0.2f;
        glm::vec3 velocity{ 0.0f };
        glm::vec3 angularVelocity{ 0.0f };
    };

    struct ColliderComponent
    {
        bool enabled = false;
        bool isSphere = true;
        bool isTrigger = false;
        glm::vec3 boxHalfExtents{ 0.6f, 0.6f, 0.6f };
        float sphereRadius = 0.75f;
    };

    struct AudioSourceComponent
    {
        bool enabled = false;
        std::string soundName = "laser";
        float volume = 0.8f;
        float pitch = 1.0f;
        bool loop = false;
        bool spatial = true;
        bool playOnAwake = false;
    };

    struct LuaScriptComponent
    {
        bool enabled = false;
        std::string scriptPath;
        bool initialized = false;
    };

    struct GameplayComponent
    {
        bool enabled = false;
        float health = 100.0f;
        float maxHealth = 100.0f;
        float shield = 100.0f;
        float maxShield = 100.0f;
        float moveSpeed = 14.0f;
        float fireRate = 0.18f;
        float fireCooldown = 0.0f;
        float damage = 25.0f;
        float lifetime = 0.0f;
        float maxLifetime = 0.0f; // > 0 means temporary entity (e.g. projectile)
        int scoreValue = 100;
        EntityID ownerId = kInvalidEntity;
        float aiTimer = 0.0f;
        int subType = 0;         // EnemyType (0..9), WeaponType (0..4), PickupType, or DoorType
        int state = 0;           // AI state or Door state (0=closed, 1=opening, 2=open)
        std::string extraKey;    // Key color ("red", "blue") or metadata
        glm::vec3 targetPos{ 0.0f }; // Door open target or lift top position
        glm::vec3 basePos{ 0.0f };   // Initial position for doors/lifts/bobbing pickups
    };

    struct Entity
    {
        EntityID id = kInvalidEntity;
        bool alive = true;

        // Godot Node3D parent-child scene graph hierarchy & process mode
        EntityID parentId = kInvalidEntity;
        std::vector<EntityID> childrenIds;
        ProcessMode processMode = ProcessMode::Inherit;

        TagComponent tag;
        TransformComponent transform;
        MeshComponent mesh;
        MaterialComponent material;
        LightComponent light;
        CameraComponent camera;
        RigidBodyComponent rigidBody;
        ColliderComponent collider;
        AudioSourceComponent audioSource;
        LuaScriptComponent luaScript;
        GameplayComponent gameplay;
    };
}
