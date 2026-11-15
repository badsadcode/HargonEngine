#include "AudioEngine.h"
#include "../core/Log.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace Hargon
{
    static constexpr float kTwoPi = 6.28318530718f;

    struct AudioEngine::Impl
    {
        static constexpr size_t kVoicePoolSize = 32;
        ma_engine engine{};
        ma_sound voices[kVoicePoolSize]{};
        bool voiceActive[kVoicePoolSize]{};
        size_t nextVoice = 0;
        uint32_t rngState = 0xA511E9B3u;
    };

    static void WriteWavFile(const std::string& path, const std::vector<int16_t>& samples, uint32_t sampleRate = 44100)
    {
        if (std::filesystem::exists(path))
            return;

        uint32_t dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
        uint32_t fileSize = 36 + dataSize;
        uint16_t audioFormat = 1; // PCM
        uint16_t numChannels = 1;
        uint32_t byteRate = sampleRate * numChannels * sizeof(int16_t);
        uint16_t blockAlign = numChannels * sizeof(int16_t);
        uint16_t bitsPerSample = 16;
        uint32_t fmtChunkSize = 16;

        std::ofstream out(path, std::ios::out | std::ios::binary);
        if (!out) return;

        out.write("RIFF", 4);
        out.write(reinterpret_cast<const char*>(&fileSize), 4);
        out.write("WAVE", 4);
        out.write("fmt ", 4);
        out.write(reinterpret_cast<const char*>(&fmtChunkSize), 4);
        out.write(reinterpret_cast<const char*>(&audioFormat), 2);
        out.write(reinterpret_cast<const char*>(&numChannels), 2);
        out.write(reinterpret_cast<const char*>(&sampleRate), 4);
        out.write(reinterpret_cast<const char*>(&byteRate), 4);
        out.write(reinterpret_cast<const char*>(&blockAlign), 2);
        out.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
        out.write("data", 4);
        out.write(reinterpret_cast<const char*>(&dataSize), 4);
        out.write(reinterpret_cast<const char*>(samples.data()), dataSize);
    }

    void AudioEngine::EnsureBuiltInSoundAssets(const std::string& dirPath)
    {
        std::filesystem::create_directories(dirPath);
        const uint32_t sr = 44100;

        // 1. laser.wav (Plasma cannon sweep 920Hz -> 180Hz)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.16f);
            std::vector<int16_t> pcm(n);
            float phase = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                float freq = 920.0f * std::pow(0.20f, t);
                phase += (kTwoPi * freq) / sr;
                float env = std::pow(1.0f - t, 1.4f);
                float sig = (std::sin(phase) + 0.35f * std::sin(phase * 2.0f)) * env * 0.45f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/laser.wav", pcm, sr);
        }

        // 2. turret_shot.wav (Quick dual-pulse laser)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.13f);
            std::vector<int16_t> pcm(n);
            float phase = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                float freq = 1250.0f * std::pow(0.32f, t);
                phase += (kTwoPi * freq) / sr;
                float env = (1.0f - t);
                float sig = std::sin(phase) * env * 0.4f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/turret_shot.wav", pcm, sr);
        }

        // 3. explosion.wav (Sub-bass drop + noise burst)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.42f);
            std::vector<int16_t> pcm(n);
            uint32_t rng = 1337u;
            float phase = 0.0f;
            float lp = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                rng = rng * 1664525u + 1013904223u;
                float white = (static_cast<float>(rng & 0xFFFF) / 32768.0f) - 1.0f;
                lp = lp * 0.85f + white * 0.15f;
                float freq = 140.0f * std::pow(0.25f, t);
                phase += (kTwoPi * freq) / sr;
                float env = std::pow(1.0f - t, 1.6f);
                float sig = (lp * 0.7f + std::sin(phase) * 0.6f) * env * 0.55f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/explosion.wav", pcm, sr);
        }

        // 4. pickup.wav (Crystal chime C6 -> E6 -> G6 -> C7)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.32f);
            std::vector<int16_t> pcm(n);
            float notes[4] = { 523.25f, 659.25f, 783.99f, 1046.50f };
            float phase = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                int step = std::min(3, static_cast<int>(t * 4.0f));
                float freq = notes[step];
                phase += (kTwoPi * freq) / sr;
                float env = std::pow(1.0f - t, 0.75f);
                float sig = (std::sin(phase) + 0.25f * std::sin(phase * 2.0f)) * env * 0.42f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/pickup.wav", pcm, sr);
        }

        // 5. hit.wav (Impact thud)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.14f);
            std::vector<int16_t> pcm(n);
            float phase = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                float freq = 260.0f * std::pow(0.3f, t);
                phase += (kTwoPi * freq) / sr;
                float env = std::pow(1.0f - t, 1.8f);
                float sig = std::sin(phase) * env * 0.5f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/hit.wav", pcm, sr);
        }

        // 6. wave_start.wav (Sci-fi alert fanfare)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.55f);
            std::vector<int16_t> pcm(n);
            float notes[3] = { 330.0f, 440.0f, 659.25f };
            float phase = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                int step = std::min(2, static_cast<int>(t * 3.0f));
                phase += (kTwoPi * notes[step]) / sr;
                float env = std::pow(1.0f - t, 0.6f);
                float sig = (std::sin(phase) + 0.3f * std::sin(phase * 1.5f)) * env * 0.4f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/wave_start.wav", pcm, sr);
        }

        // 7. victory.wav (Victory fanfare)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.85f);
            std::vector<int16_t> pcm(n);
            float notes[4] = { 440.0f, 554.37f, 659.25f, 880.0f };
            float phase = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                int step = std::min(3, static_cast<int>(t * 4.0f));
                phase += (kTwoPi * notes[step]) / sr;
                float env = std::pow(1.0f - t * 0.8f, 0.7f);
                float sig = (std::sin(phase) + 0.35f * std::sin(phase * 2.0f)) * env * 0.42f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/victory.wav", pcm, sr);
        }

        // 8. axe_swing.wav (Heavy metallic whoosh + cleave)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.18f);
            std::vector<int16_t> pcm(n);
            uint32_t rng = 4201u;
            float lp = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                rng = rng * 1664525u + 1013904223u;
                float white = (static_cast<float>(rng & 0xFFFF) / 32768.0f) - 1.0f;
                lp = lp * 0.78f + white * 0.22f;
                float env = std::sin(t * 3.14159f);
                float sig = lp * env * 0.55f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/axe_swing.wav", pcm, sr);
        }

        // 9. shotgun.wav (Booming Quake double-barrel blast + pump clack)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.34f);
            std::vector<int16_t> pcm(n);
            uint32_t rng = 9001u;
            float phase = 0.0f, lp = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                rng = rng * 1664525u + 1013904223u;
                float white = (static_cast<float>(rng & 0xFFFF) / 32768.0f) - 1.0f;
                lp = lp * 0.80f + white * 0.20f;
                float freq = 185.0f * std::pow(0.22f, t);
                phase += (kTwoPi * freq) / sr;
                float env = std::pow(1.0f - t, 1.5f);
                float pump = (t > 0.65f && t < 0.75f) ? 0.35f * white : 0.0f;
                float sig = ((lp * 0.85f + std::sin(phase) * 0.75f) * env + pump) * 0.65f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/shotgun.wav", pcm, sr);
        }

        // 10. nailgun.wav (Punchy metallic spike driver)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.09f);
            std::vector<int16_t> pcm(n);
            uint32_t rng = 777u;
            float phase = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                rng = rng * 1664525u + 1013904223u;
                float white = (static_cast<float>(rng & 0xFFFF) / 32768.0f) - 1.0f;
                float freq = 640.0f * std::pow(0.28f, t);
                phase += (kTwoPi * freq) / sr;
                float env = std::pow(1.0f - t, 1.4f);
                float sig = (std::sin(phase) * 0.7f + white * 0.4f) * env * 0.58f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/nailgun.wav", pcm, sr);
        }

        // 11. rocket_fire.wav (Deep tube thump + rocket thruster hiss)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.28f);
            std::vector<int16_t> pcm(n);
            uint32_t rng = 31337u;
            float phase = 0.0f, lp = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                rng = rng * 1664525u + 1013904223u;
                float white = (static_cast<float>(rng & 0xFFFF) / 32768.0f) - 1.0f;
                lp = lp * 0.72f + white * 0.28f;
                float freq = 130.0f * std::pow(0.45f, t);
                phase += (kTwoPi * freq) / sr;
                float env = std::pow(1.0f - t, 1.1f);
                float sig = (std::sin(phase) * 0.65f + lp * 0.65f) * env * 0.60f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/rocket_fire.wav", pcm, sr);
        }

        // 12. lightning.wav (High-voltage crackling Thunderbolt discharge)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.11f);
            std::vector<int16_t> pcm(n);
            uint32_t rng = 55123u;
            float phase = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                rng = rng * 1664525u + 1013904223u;
                float white = (static_cast<float>(rng & 0xFFFF) / 32768.0f) - 1.0f;
                phase += (kTwoPi * (120.0f + 880.0f * std::abs(white))) / sr;
                float env = 1.0f - 0.5f * t;
                float sig = (std::sin(phase) * 0.6f + white * 0.5f) * env * 0.50f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/lightning.wav", pcm, sr);
        }

        // 13. gib_splat.wav (Quake meaty overkill gibbing crunch & wet splat)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.30f);
            std::vector<int16_t> pcm(n);
            uint32_t rng = 6661u;
            float phase = 0.0f, lp = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                rng = rng * 1664525u + 1013904223u;
                float white = (static_cast<float>(rng & 0xFFFF) / 32768.0f) - 1.0f;
                lp = lp * 0.84f + white * 0.16f;
                float freq = 95.0f + 45.0f * std::sin(t * 38.0f);
                phase += (kTwoPi * freq) / sr;
                float env = std::pow(1.0f - t, 1.2f);
                float sig = (std::sin(phase) * 0.7f + lp * 0.8f) * env * 0.65f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/gib_splat.wav", pcm, sr);
        }

        // 14. door_open.wav (Heavy stone/iron mechanism grinding open)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.45f);
            std::vector<int16_t> pcm(n);
            uint32_t rng = 8844u;
            float phase = 0.0f, lp = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                rng = rng * 1664525u + 1013904223u;
                float white = (static_cast<float>(rng & 0xFFFF) / 32768.0f) - 1.0f;
                lp = lp * 0.90f + white * 0.10f;
                phase += (kTwoPi * 72.0f) / sr;
                float env = std::sin(t * 3.14159f);
                float sig = (std::sin(phase) * 0.6f + lp * 0.7f) * env * 0.50f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/door_open.wav", pcm, sr);
        }

        // 15. monster_roar.wav (Deep guttural monster aggro growl)
        {
            uint32_t n = static_cast<uint32_t>(sr * 0.36f);
            std::vector<int16_t> pcm(n);
            float phase = 0.0f;
            for (uint32_t i = 0; i < n; ++i)
            {
                float t = static_cast<float>(i) / n;
                float freq = 115.0f * (1.0f - 0.35f * t) + 18.0f * std::sin(t * 55.0f);
                phase += (kTwoPi * freq) / sr;
                float env = std::sin(t * 3.14159f);
                float sig = (std::sin(phase) + 0.5f * std::sin(phase * 2.0f) + 0.3f * std::sin(phase * 3.0f)) * env * 0.38f;
                pcm[i] = static_cast<int16_t>(std::clamp(sig, -1.0f, 1.0f) * 32000.0f);
            }
            WriteWavFile(dirPath + "/monster_roar.wav", pcm, sr);
        }
    }

    AudioEngine::~AudioEngine()
    {
        Shutdown();
    }

    bool AudioEngine::Init(const std::string& audioAssetDir)
    {
        m_AudioDir = audioAssetDir;
        EnsureBuiltInSoundAssets(m_AudioDir);

        m_SoundNames = {
            "laser",
            "turret_shot",
            "explosion",
            "pickup",
            "hit",
            "wave_start",
            "victory",
            "axe_swing",
            "shotgun",
            "nailgun",
            "rocket_fire",
            "lightning",
            "gib_splat",
            "door_open",
            "monster_roar"
        };

        m_Impl = new Impl();
        ma_engine_config config = ma_engine_config_init();
        ma_result res = ma_engine_init(&config, &m_Impl->engine);
        if (res != MA_SUCCESS)
        {
            // Try no-device (headless/CI fallback) so miniaudio still runs cleanly on machines without speakers
            config.noDevice = MA_TRUE;
            config.channels = 2;
            config.sampleRate = 44100;
            res = ma_engine_init(&config, &m_Impl->engine);
            if (res != MA_SUCCESS)
            {
                Log::Audio("miniaudio could not open audio device; continuing in silent mode.");
                delete m_Impl;
                m_Impl = nullptr;
                m_Initialized = false;
                return false;
            }
            Log::Audio("Initialized miniaudio engine (headless/virtual device mode).");
        }
        else
        {
            Log::Audio("Initialized miniaudio engine (hardware audio playback active).");
        }

        m_Initialized = true;
        ma_engine_set_volume(&m_Impl->engine, m_MasterVolume);
        return true;
    }

    void AudioEngine::Shutdown()
    {
        if (m_Impl)
        {
            if (m_Initialized)
            {
                for (size_t i = 0; i < Impl::kVoicePoolSize; ++i)
                {
                    if (m_Impl->voiceActive[i])
                    {
                        ma_sound_uninit(&m_Impl->voices[i]);
                        m_Impl->voiceActive[i] = false;
                    }
                }
                ma_engine_uninit(&m_Impl->engine);
            }
            delete m_Impl;
            m_Impl = nullptr;
        }
        m_Initialized = false;
    }

    std::string AudioEngine::ResolveSoundPath(const std::string& soundName) const
    {
        if (soundName.find(".wav") != std::string::npos || soundName.find('/') != std::string::npos || soundName.find('\\') != std::string::npos)
        {
            if (std::filesystem::exists(soundName))
                return soundName;
        }
        std::string candidate = m_AudioDir + "/" + soundName + ".wav";
        if (std::filesystem::exists(candidate))
            return candidate;
        return m_AudioDir + "/laser.wav";
    }

    void AudioEngine::PlaySound2D(const std::string& soundName, float volume, float pitch)
    {
        ++m_PlayedSoundCount;
        if (!m_Initialized || m_Muted || !m_Impl)
            return;

        std::string path = ResolveSoundPath(soundName);
        size_t slot = m_Impl->nextVoice;
        m_Impl->nextVoice = (m_Impl->nextVoice + 1) % Impl::kVoicePoolSize;

        if (m_Impl->voiceActive[slot])
        {
            ma_sound_uninit(&m_Impl->voices[slot]);
            m_Impl->voiceActive[slot] = false;
        }

        if (ma_sound_init_from_file(&m_Impl->engine, path.c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, nullptr, nullptr, &m_Impl->voices[slot]) == MA_SUCCESS)
        {
            m_Impl->voiceActive[slot] = true;
            ma_sound_set_spatialization_enabled(&m_Impl->voices[slot], MA_FALSE);
            ma_sound_set_volume(&m_Impl->voices[slot], std::clamp(volume * m_SFXVolume, 0.0f, 2.0f));
            ma_sound_set_pitch(&m_Impl->voices[slot], std::clamp(pitch, 0.25f, 3.0f));
            ma_sound_start(&m_Impl->voices[slot]);
        }
        else
        {
            ma_engine_play_sound(&m_Impl->engine, path.c_str(), nullptr);
        }
    }

    void AudioEngine::PlaySound3D(
        const std::string& soundName,
        const glm::vec3& position,
        float volume,
        float pitch,
        int attenuationModel,
        float unitSize,
        float maxDistance,
        float pitchRandomness)
    {
        float dist = glm::length(position - m_ListenerPos);
        if (maxDistance > 0.0f && dist > maxDistance && attenuationModel != 3)
            return;

        // Compute Godot AudioStreamPlayer3D-inspired pitch randomization + attenuation gain
        float finalPitch = pitch;
        if (pitchRandomness > 0.001f && m_Impl)
        {
            m_Impl->rngState = m_Impl->rngState * 1664525u + 1013904223u;
            float u = (static_cast<float>(m_Impl->rngState & 0xFFFFu) / 32768.0f) - 1.0f;
            finalPitch = std::clamp(pitch + u * pitchRandomness, 0.35f, 2.5f);
        }

        float refDist = std::max(0.5f, unitSize);
        float attenGain = 1.0f;
        if (attenuationModel == 0) // InverseDistance
            attenGain = refDist / std::max(refDist, dist);
        else if (attenuationModel == 1) // InverseSquare
            attenGain = (refDist * refDist) / std::max(refDist * refDist, dist * dist);
        else if (attenuationModel == 2) // Logarithmic
            attenGain = std::clamp(1.0f - std::log10(std::max(1.0f, dist / refDist + 1.0f)) * 0.45f, 0.05f, 1.0f);

        ++m_PlayedSoundCount;
        if (!m_Initialized || m_Muted || !m_Impl)
            return;

        std::string path = ResolveSoundPath(soundName);
        size_t slot = m_Impl->nextVoice;
        m_Impl->nextVoice = (m_Impl->nextVoice + 1) % Impl::kVoicePoolSize;

        if (m_Impl->voiceActive[slot])
        {
            ma_sound_uninit(&m_Impl->voices[slot]);
            m_Impl->voiceActive[slot] = false;
        }

        if (ma_sound_init_from_file(&m_Impl->engine, path.c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, nullptr, nullptr, &m_Impl->voices[slot]) == MA_SUCCESS)
        {
            m_Impl->voiceActive[slot] = true;
            ma_sound_set_spatialization_enabled(&m_Impl->voices[slot], MA_TRUE);
            ma_sound_set_position(&m_Impl->voices[slot], position.x, position.y, position.z);
            ma_sound_set_attenuation_model(&m_Impl->voices[slot], ma_attenuation_model_none); // exact curve applied via attenGain
            ma_sound_set_volume(&m_Impl->voices[slot], std::clamp(volume * attenGain * m_SFXVolume, 0.0f, 2.0f));
            ma_sound_set_pitch(&m_Impl->voices[slot], finalPitch);
            ma_sound_start(&m_Impl->voices[slot]);
        }
        else
        {
            ma_engine_play_sound(&m_Impl->engine, path.c_str(), nullptr);
        }
    }

    void AudioEngine::SetListenerTransform(const glm::vec3& position, const glm::vec3& forward)
    {
        m_ListenerPos = position;
        if (m_Initialized && m_Impl)
        {
            ma_engine_listener_set_position(&m_Impl->engine, 0, position.x, position.y, position.z);
            ma_engine_listener_set_direction(&m_Impl->engine, 0, forward.x, forward.y, forward.z);
        }
    }

    void AudioEngine::SetMasterVolume(float volume)
    {
        m_MasterVolume = std::clamp(volume, 0.0f, 1.0f);
        if (m_Initialized && m_Impl)
            ma_engine_set_volume(&m_Impl->engine, m_Muted ? 0.0f : m_MasterVolume);
    }

    void AudioEngine::SetMuted(bool muted)
    {
        m_Muted = muted;
        if (m_Initialized && m_Impl)
            ma_engine_set_volume(&m_Impl->engine, m_Muted ? 0.0f : m_MasterVolume);
    }
}
