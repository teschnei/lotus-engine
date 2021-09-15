#pragma once

#include <filesystem>
#include <array>
#include <unordered_map>
#include "engine/audio.h"

namespace SoLoud
{
    class AudioSource;
}

namespace lotus
{
    class Engine;
}

namespace FFXI
{
    class Audio
    {
    public:
        Audio(lotus::Engine* _engine);
        std::optional<lotus::AudioEngine::AudioInstance> playSound(uint32_t id);
        void setMusic(uint32_t id, uint8_t type);
    private:
        std::unique_ptr<SoLoud::AudioSource> loadSound(uint32_t id);
        std::unique_ptr<SoLoud::AudioSource> loadMusic(uint32_t id);
        std::unique_ptr<SoLoud::AudioSource> loadAudioSource(std::filesystem::path);

        std::array<std::filesystem::path, 16> sound_paths;
        std::array<std::unique_ptr<SoLoud::AudioSource>, 5> bgm;
        std::unordered_map<uint32_t, std::unique_ptr<SoLoud::AudioSource>> sounds;
        std::optional<lotus::AudioEngine::AudioInstance> bgm_instance;
        lotus::Engine* engine;
    };
}
