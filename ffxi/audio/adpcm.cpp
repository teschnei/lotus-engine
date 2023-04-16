#include "adpcm.h"

#include <cstring>
#include <cstddef>

ADPCMInstance::ADPCMInstance(ADPCM* adpcm) : adpcm(adpcm)
{
}

unsigned int ADPCMInstance::getAudio(float* buffer, unsigned int samples, unsigned int buffer_size)
{
    if (!adpcm)
        return 0;

    size_t write_size = samples > adpcm->samples - offset ? adpcm->samples - offset : samples;

    for (const auto& channel_data : adpcm->data)
    {
        memcpy(buffer, channel_data.data() + offset, sizeof(float) * write_size);
    }

    offset += write_size;

    if (offset >= adpcm->samples && (mFlags & AudioSourceInstance::LOOPING))
    {
        offset = adpcm->loop_start * adpcm->block_size;
    }

    return write_size;
}

bool ADPCMInstance::hasEnded()
{
    if (!(mFlags & AudioSourceInstance::LOOPING) && offset >= adpcm->samples)
        return true;
    return false;
}

ADPCM::ADPCM(std::ifstream&& file, uint32_t _blocks, uint32_t _block_size, uint32_t _loop_start, uint32_t _channels, float _sample_rate) :
    samples(_blocks * _block_size), loop_start(_loop_start), block_size(_block_size)
{
    mChannels = _channels;
    mBaseSamplerate = _sample_rate;

    std::vector<int> decoder_state;
    decoder_state.resize(mChannels * 2);

    data.resize(mChannels);
    for (auto& channel_data : data)
    {
        channel_data.reserve(samples);
    }

    std::vector<std::byte> block;
    block.resize((1 + block_size / 2) * mChannels);
    while (file.good())
    {
        file.read((char*)block.data(), (1 + block_size / 2) * mChannels);
        std::byte* compressed_block_start = block.data();
        for (size_t channel = 0; channel < mChannels; channel++)
        {
            int base_index = channel * (1 + block_size / 2);
            int scale = 0x0C - std::to_integer<int>((compressed_block_start[base_index] & std::byte{ 0b1111 }));
            int index = std::to_integer<size_t>(compressed_block_start[base_index] >> 4);
            if (index < 5)
            {
                for (uint8_t sample = 0; sample < (block_size / 2); ++sample)
                {
                    std::byte sample_byte = compressed_block_start[base_index + sample + 1];
                    for (uint8_t nibble = 0; nibble < 2; ++nibble)
                    {
                        int value = std::to_integer<int>(sample_byte >> (4 * nibble) & std::byte(0b1111));
                        if (value >= 8) value -= 16;
                        value <<= scale;
                        value += (decoder_state[channel * 2] * filter0[index] + decoder_state[channel * 2 + 1] * filter1[index]) / 256;
                        decoder_state[channel * 2 + 1] = decoder_state[channel * 2];
                        decoder_state[channel * 2] = value > 0x7FFF ? 0x7FFF : value < -0x8000 ? -0x8000 : value;
                        data[channel].push_back(int16_t(decoder_state[channel * 2]) / float(0x8000));
                    }
                }
            }
        }
    }
}

SoLoud::AudioSourceInstance* ADPCM::createInstance()
{
    return new ADPCMInstance(this);
}
