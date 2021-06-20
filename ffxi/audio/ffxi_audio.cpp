#include "ffxi_audio.h"

#include <fstream>
#include "adpcm.h"
#include "adpcmstream.h"
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

std::unique_ptr<SoLoud::AudioSource> FFXI::Audio::loadSound(std::filesystem::path path)
{
    std::vector<std::byte> data;
    data.resize(8);
    auto file = std::ifstream{ path, std::ios::binary };

    if (!file.good())
        throw std::runtime_error("file not found: " + path.string());

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
