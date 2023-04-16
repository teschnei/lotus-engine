#include "ffxi_audio.h"

#include <fstream>
#include <format>

#include "engine/core.h"

#include "adpcm.h"
#include "adpcmstream.h"
#include "config.h"
#include "pcm.h"

enum class SampleFormat : uint32_t
{
    ADPCM = 0,
    PCM = 1,
    ATRAC3 = 3
};

struct BGMHeader
{
    char header[12];
    SampleFormat sample_format;
    uint32_t size;
    uint32_t id;
    uint32_t sample_blocks;
    uint32_t loop_start;
    uint32_t sample_rate_high;
    uint32_t sample_rate_low;
    uint32_t unknown1;
    uint8_t unknown2;
    uint8_t unknown3;
    uint8_t channels;
    uint8_t blocksize;
};

struct SoundEffectHeader
{
    char header[8];
    uint32_t size;
    SampleFormat sample_format;
    uint32_t id;
    uint32_t sample_blocks;
    uint32_t loop_start;
    uint32_t sample_rate_high;
    uint32_t sample_rate_low;
    uint32_t unknown1;
    uint8_t unknown2;
    uint8_t unknown3;
    uint8_t channels;
    uint8_t blocksize;
    uint32_t unknown4;
};

FFXI::Audio::Audio(lotus::Engine* _engine) : engine(_engine)
{
    auto base_path = static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path;
    sound_paths[0] = base_path / "sound/win";
    for (auto i = 1u; i < 16u; ++i)
    {
        sound_paths[i] = base_path / std::format("sound{}/win", i);
    }
}

std::optional<lotus::AudioEngine::AudioInstance> FFXI::Audio::playSound(uint32_t id)
{
    if (!sounds.contains(id))
    {
        if (auto se = loadSound(id))
        {
            sounds[id] = std::move(se);
        }
    }
    if (sounds.contains(id))
    {
        return engine->audio->playSound(*sounds[id]);
    }
    return {};
}

void FFXI::Audio::setMusic(uint32_t id, uint8_t type)
{
    if (auto music = loadMusic(id); music && type < bgm.size())
    {
        bgm[type] = std::move(music);

        //TODO: check if this is the current music to play (day/night/battle/party/mount)
        bgm_instance = engine->audio->playBGM(*bgm[type]);
    }
}

std::unique_ptr<SoLoud::AudioSource> FFXI::Audio::loadSound(uint32_t id)
{
    for (const auto& path : sound_paths)
    {
        if (auto as = loadAudioSource(path / std::format("se/se{:03}/se{:06}.spw", id / 1000, id)))
        {
            return as;
        }
    }
    throw std::runtime_error("sound not found: " + id);
}

std::unique_ptr<SoLoud::AudioSource> FFXI::Audio::loadMusic(uint32_t id)
{
    for (const auto& path : sound_paths)
    {
        if (auto as = loadAudioSource(path / std::format("music/data/music{:03}.bgw", id)))
        {
            return as;
        }
    }
    throw std::runtime_error("music not found: " + id);
}

std::unique_ptr<SoLoud::AudioSource> FFXI::Audio::loadAudioSource(std::filesystem::path path)
{
    auto file = std::ifstream{ path, std::ios::binary };

    if (!file.good())
        return nullptr;

    std::vector<std::byte> data;
    data.resize(8);

    file.read((char*)data.data(), 8);
    if (std::string((const char*)data.data(), 8).starts_with("SeWave"))
    {
        data.resize(sizeof(SoundEffectHeader));
        file.read((char*)data.data() + 8, sizeof(SoundEffectHeader) - 8);
        SoundEffectHeader* header = (SoundEffectHeader*)(data.data());
        float sample_rate = (header->sample_rate_low + header->sample_rate_high);
        if (header->sample_format == SampleFormat::ADPCM)
        {
            return std::make_unique<ADPCM>(std::move(file), header->sample_blocks, header->blocksize, header->loop_start, header->channels, sample_rate);
        }
        else if (header->sample_format == SampleFormat::PCM)
        {
            return std::make_unique<PCM>(std::move(file), header->sample_blocks, header->blocksize, header->loop_start, header->channels, sample_rate);
        }
    }
    else if (std::string((const char*)data.data(), 8).starts_with("BGMStrea"))
    {
        data.resize(sizeof(BGMHeader));
        file.read((char*)data.data() + 8, sizeof(BGMHeader) - 8);
        BGMHeader* header = (BGMHeader*)(data.data());
        float sample_rate = (header->sample_rate_low + header->sample_rate_high);
        return std::make_unique<ADPCMStream>(std::move(file), header->sample_blocks, header->blocksize, header->loop_start, header->channels, sample_rate);
    }
    return nullptr;
}
