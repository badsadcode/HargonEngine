#pragma once
#include "../scene/Scene.h"
#include "../renderer/Renderer3D.h"
#include "../audio/AudioEngine.h"
#include "../core/Window.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>

namespace Hargon
{
    enum class FPSWeaponType : int
    {
        RunicAxe = 0,
        Shotgun = 1,
        SuperNailgun = 2,
        RocketLauncher = 3,
        LightningGun = 4,
        Count = 5
    };

    enum class FPSEnemyType : int
    {
        RottedGrunt = 0,
        Enforcer = 1,
        Rottweiler = 2,
        IronKnight = 3,
        HellKnight = 4,
        ChainsawOgre = 5,
        Fiend = 6,
        Scrag = 7,
        Vore = 8,
        Shambler = 9,
        Count = 10
    };

    enum class FPSPickupType : int
    {
        Health = 0,
        MegaHealth = 1,
        Armor = 2,
        AmmoShells = 3,
        AmmoNails = 4,
        AmmoRockets = 5,
        AmmoCells = 6,
        WeaponShotgun = 7,
        WeaponSuperNailgun = 8,
        WeaponRocketLauncher = 9,
        WeaponLightningGun = 10,
        KeyRed = 11,
        KeyBlue = 12
    };

    enum class FPSMenuState
    {
        None = 0,
        MainMenu,
        CampaignSelect,
        SaveLoadMenu,
        SettingsMenu,
        PauseMenu,
        LevelComplete,
        GameOver,
        CampaignVictory
    };

    struct FPSSettings
    {
        // Graphics settings
        float fovDegrees = 78.0f;
        float brightness = 1.05f;
        float fogMultiplier = 1.0f;
        bool dynamicLights = true;
        float bloodGoreScale = 1.0f;
        bool fullscreen = false;
        int resolutionIndex = 1; // 0=1280x720, 1=1600x900, 2=1920x1080

        // Sound & Input settings
        float masterVolume = 0.85f;
        float sfxVolume = 0.90f;
        float ambientVolume = 0.65f;
        bool muted = false;
        float mouseSensitivity = 0.18f;
    };

    struct FPSSaveSlotInfo
    {
        int slotIndex = 0;
        bool exists = false;
        std::string title = "Empty Slot";
        std::string timestamp;
        int campaign = 1;
        int levelIndex = 1;
        int health = 100;
        int armor = 0;
    };

    struct BadLevelSpan
    {
        float floorY = 0.0f;
        float ceilY = 4.0f;
    };

    struct BadLevelRoomInfo
    {
        int id = 0;
        std::string kind;
        int x = 0, y = 0, w = 0, h = 0;
        float floorY = 0.0f;
        float ceilY = 4.0f;
    };

    class FPSGameManager
    {
    public:
        FPSGameManager() = default;
        ~FPSGameManager() = default;

        void Init(Scene* scene, Renderer3D* renderer, AudioEngine* audio, Window* window, bool startInMenu = true);

        // Loads one of the 10 BadLevel campaign maps (campaign 1..2, levelIndex 1..5)
        bool LoadCampaignLevel(int campaign, int levelIndex, bool resetPlayerInventory = false);

        // Generates a fresh BadLevel map on the fly with a custom seed and loads it
        bool GenerateAndLoadLevel(int campaign, int levelIndex, int customSeed);

        // Steps first-person movement, weapons, projectiles, 10 enemy types, doors, lifts, gibs & blood spatter
        void Step(float dt);

        // Updates the first-person camera and weapon viewmodel entity transform
        void UpdateFirstPersonCamera(CameraView& ioCamera, float dt);

        // Renders Quake-1 style HUD, damage/pickup flashes, crosshair, and Menus (Main/Pause/Settings/Save/Load)
        void RenderUI(const ImVec2& vpMin, const ImVec2& vpSize, ImDrawList* dl);

        // Save / Load system (5 slots + QuickSave slot 0)
        bool SaveGame(int slotIndex, std::string* outMsg = nullptr);
        bool LoadGame(int slotIndex, std::string* outMsg = nullptr);
        void RefreshSaveSlots();

        // Settings persistence
        void LoadSettings(const std::string& path = "settings.json");
        void SaveSettings(const std::string& path = "settings.json");
        void ApplySettings();

        // Collision & World Queries against BadLevel span grid + doors
        bool QueryColumnFloorCeil(float worldX, float worldZ, float currentFeetY, float& outFloorY, float& outCeilY) const;
        bool CanMoveTo(const glm::vec3& fromFeet, const glm::vec3& toFeet, float radius, float height, float maxStepUp, glm::vec3& outResolvedFeet, bool& outGrounded) const;
        bool RaycastWorldSolid(const glm::vec3& origin, const glm::vec3& dir, float maxDist, float& outDist, glm::vec3& outHitPos, glm::vec3& outHitNormal) const;

        // Blood & Gore System (Quake 1 style)
        void SpawnBloodSpatter(const glm::vec3& hitPos, const glm::vec3& hitNormal, int count = 14, bool spawnWallFloorDecals = true);
        void SpawnMeatExplosionGibs(const glm::vec3& pos, int gibCount = 7);

        // Accessors
        FPSMenuState GetMenuState() const { return m_MenuState; }
        void SetMenuState(FPSMenuState state);
        bool IsInMenu() const { return m_MenuState != FPSMenuState::None; }

        int GetCurrentCampaign() const { return m_Campaign; }
        int GetCurrentLevelIndex() const { return m_LevelIndex; }
        const std::string& GetCurrentLevelCode() const { return m_LevelCode; }
        const std::string& GetCurrentLevelTitle() const { return m_LevelTitle; }
        const std::string& GetCurrentStyle() const { return m_LevelStyle; }

        const glm::vec3& GetPlayerFeetPos() const { return m_PlayerFeetPos; }
        void SetPlayerPose(const glm::vec3& feetPos, float yawDeg, float pitchDeg);
        float GetPlayerYaw() const { return m_PlayerYaw; }
        float GetPlayerPitch() const { return m_PlayerPitch; }
        float GetPlayerHealth() const { return m_PlayerHealth; }
        float GetPlayerArmor() const { return m_PlayerArmor; }

        int GetCurrentWeapon() const { return static_cast<int>(m_CurrentWeapon); }
        void SelectWeapon(int weaponIndex);
        void FireCurrentWeapon();
        void UnlockAllWeaponsForShowcase();
        void AimAtNearestVisibleEnemy();

        int GetTotalKills() const { return m_KillsInLevel; }
        int GetTotalMonsters() const { return m_TotalMonstersInLevel; }
        int GetSecretsFound() const { return m_SecretsFound; }
        int GetTotalSecrets() const { return m_TotalSecrets; }
        int GetBloodDecalCount() const { return m_BloodDecalCount; }

        FPSSettings& GetSettings() { return m_Settings; }
        const std::array<FPSSaveSlotInfo, 6>& GetSaveSlots() const { return m_SaveSlots; }

        static const char* GetWeaponName(FPSWeaponType type);
        static const char* GetEnemyName(FPSEnemyType type);
        static const char* GetCampaignTitle(int campaign);
        static std::string GetLevelCode(int campaign, int levelIndex);

    private:
        void SpawnEnemyByType(FPSEnemyType type, const glm::vec3& feetPos, float yaw = 0.0f);
        void SpawnAtmosphericDecorations();
        void SpawnPickupEntity(FPSPickupType type, const glm::vec3& pos);
        void DamageEnemy(Entity& enemy, float damage, const glm::vec3& hitPoint, const glm::vec3& knockbackDir, bool canGib);
        void DamagePlayer(float damage, const glm::vec3& sourcePos);
        void ExplodeAt(const glm::vec3& pos, float radius, float maxDamage, EntityID ignoreOwner);
        void EnsureViewModelEntity();

    private:
        Scene* m_Scene = nullptr;
        Renderer3D* m_Renderer = nullptr;
        AudioEngine* m_Audio = nullptr;
        Window* m_Window = nullptr;

        // Campaign & Level Metadata
        int m_Campaign = 1;       // 1 = The Elder Ward, 2 = The Iron Citadel
        int m_LevelIndex = 1;     // 1..5
        int m_LevelSeed = 101;
        std::string m_LevelCode = "c1m1";
        std::string m_LevelTitle = "The Slipgate Sanctum";
        std::string m_LevelStyle = "gothic";
        std::string m_LevelRecipe = "classic";

        // BadLevel Grid Spans & Diagonals for fast 3D collision & stair climbing
        int m_GridWidth = 0;
        int m_GridHeight = 0;
        std::unordered_map<uint64_t, std::vector<BadLevelSpan>> m_ColumnSpans;
        std::unordered_map<uint64_t, std::string> m_DiagonalCorners;
        std::vector<BadLevelRoomInfo> m_Rooms;

        // First-Person Player State
        glm::vec3 m_PlayerFeetPos{ 0.0f, 1.0f, 0.0f };
        glm::vec3 m_PlayerVelocity{ 0.0f };
        float m_PlayerYaw = 0.0f;
        float m_PlayerPitch = 0.0f;
        bool m_PlayerGrounded = true;
        float m_EyeHeight = 1.45f;
        float m_HeadBobPhase = 0.0f;
        float m_WeaponSwayPhase = 0.0f;
        float m_WeaponRecoil = 0.0f;
        float m_WeaponFireTimer = 0.0f;
        float m_MuzzleFlashTimer = 0.0f;

        float m_PlayerHealth = 100.0f;
        float m_PlayerMaxHealth = 100.0f;
        float m_PlayerArmor = 50.0f;
        float m_PlayerMaxArmor = 150.0f;

        // 5 Weapons & Ammo
        FPSWeaponType m_CurrentWeapon = FPSWeaponType::Shotgun;
        std::array<bool, 5> m_WeaponUnlocked{ true, true, true, false, false };
        // Ammo: 0=Infinite(Axe), 1=Shells, 2=Nails, 3=Rockets, 4=Cells
        std::array<int, 5> m_Ammo{ 999, 36, 80, 12, 40 };
        std::array<int, 5> m_MaxAmmo{ 999, 100, 200, 50, 100 };

        // Keys held
        bool m_HasRedKey = false;
        bool m_HasBlueKey = false;

        // Level Progress Stats
        int m_KillsInLevel = 0;
        int m_TotalMonstersInLevel = 0;
        int m_SecretsFound = 0;
        int m_TotalSecrets = 0;
        float m_LevelTime = 0.0f;
        int m_BloodDecalCount = 0;
        int m_MaxBloodDecals = 96;

        // Screen Flashes & Notifications
        float m_DamageFlashAlpha = 0.0f;
        float m_PickupFlashAlpha = 0.0f;
        std::string m_StatusMessage;
        float m_StatusMessageTimer = 0.0f;

        // Viewmodel & Muzzle Light Entities
        EntityID m_ViewModelEntity = kInvalidEntity;
        EntityID m_PlayerLightEntity = kInvalidEntity;

        // Menu & Save/Load State
        FPSMenuState m_MenuState = FPSMenuState::MainMenu;
        FPSMenuState m_PreviousMenuState = FPSMenuState::MainMenu;
        bool m_SaveMenuIsSaving = false;
        int m_SelectedCampaignTab = 1;
        FPSSettings m_Settings;
        std::array<FPSSaveSlotInfo, 6> m_SaveSlots;
    };
}
