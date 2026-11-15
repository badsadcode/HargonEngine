#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace Hargon
{
    class AudioEngine
    {
    public:
        AudioEngine() = default;
        ~AudioEngine();

        bool Init(const std::string& audioAssetDir = "assets/audio");
        void Shutdown();

        void PlaySound2D(const std::string& soundName, float volume = 0.8f, float pitch = 1.0f);
        void PlaySound3D(
            const std::string& soundName,
            const glm::vec3& position,
            float volume = 0.85f,
            float pitch = 1.0f,
            int attenuationModel = 0,
            float unitSize = 10.0f,
            float maxDistance = 65.0f,
            float pitchRandomness = 0.0f);

        void SetListenerTransform(const glm::vec3& position, const glm::vec3& forward);
        void SetMasterVolume(float volume);
        float GetMasterVolume() const { return m_MasterVolume; }

        void SetSFXVolume(float volume) { m_SFXVolume = volume; }
        float GetSFXVolume() const { return m_SFXVolume; }

        void SetAmbientVolume(float volume) { m_AmbientVolume = volume; }
        float GetAmbientVolume() const { return m_AmbientVolume; }

        void SetMuted(bool muted);
        bool IsMuted() const { return m_Muted; }
        bool IsInitialized() const { return m_Initialized; }

        uint32_t GetPlayedSoundCount() const { return m_PlayedSoundCount; }
        const std::vector<std::string>& GetAvailableSounds() const { return m_SoundNames; }

        static void EnsureBuiltInSoundAssets(const std::string& dirPath);

    private:
        std::string ResolveSoundPath(const std::string& soundName) const;

    private:
        struct Impl;
        Impl* m_Impl = nullptr;
        bool m_Initialized = false;
        bool m_Muted = false;
        float m_MasterVolume = 0.80f;
        float m_SFXVolume = 0.85f;
        float m_AmbientVolume = 0.65f;
        std::string m_AudioDir = "assets/audio";
        glm::vec3 m_ListenerPos{ 0.0f };
        uint32_t m_PlayedSoundCount = 0;
        std::vector<std::string> m_SoundNames;
    };
}
