#include "pcm.h"

#include <algorithm>
#include <cstring>

PCMInstance::PCMInstance(PCM* pcm) : pcm(pcm)
{
}

unsigned int PCMInstance::getAudio(float* buffer, unsigned int samples, unsigned int buffer_size)
{
    if (!pcm)
        return 0;

    uint32_t length = samples > pcm->data[0].size() - offset ? pcm->data[0].size() - offset : samples;

    for (const auto& channel_data : pcm->data)
    {
        memcpy(buffer, channel_data.data() + offset, sizeof(float) * length);
    }

    offset += length;

    return length;
}

bool PCMInstance::hasEnded()
{
    if (!(mFlags & AudioSourceInstance::LOOPING) && offset >= pcm->data[0].size())
    {
        return true;
    }
    return false;
}

PCM::PCM(std::ifstream&& file, uint32_t _samples, uint32_t block_size, uint32_t _loop_start, uint32_t _channels, float _sample_rate) :
    samples(_samples), loop_start(_loop_start)
{
    mChannels = _channels;
    mBaseSamplerate = _sample_rate;
    data.resize(mChannels);
    for (auto& channel_data : data)
    {
        channel_data.reserve(samples);
    }

    std::vector<int16_t> block;
    block.resize(block_size);
    while (file.good())
    {
        for (auto& channel_data : data)
        {
            file.read((char*)block.data(), block_size * sizeof(int16_t));
            std::ranges::transform(block, std::back_inserter(channel_data), [](auto& u) {return u / float(0x8000); });
        }
    }
}

SoLoud::AudioSourceInstance* PCM::createInstance()
{
    return new PCMInstance(this);
}
