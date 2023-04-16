#pragma once
#include <fstream>
#include <array>
#include <vector>
#define SOLOUD_NO_ASSERTS
#include <soloud.h>
#include "engine/types.h"

class ADPCMStream;

class ADPCMStreamInstance : public SoLoud::AudioSourceInstance
{
public:
    ADPCMStreamInstance(ADPCMStream*);
    virtual unsigned int getAudio(float* buffer, unsigned int samples, unsigned int buffer_size);
    virtual bool hasEnded();
private:
    std::vector<float> data;
    size_t offset{ 0 };
    ADPCMStream* adpcm;
};

class ADPCMStream : public SoLoud::AudioSource
{
public:
    ADPCMStream(std::ifstream&& file, uint32_t _blocks, uint32_t _block_size, uint32_t _loop_start, uint32_t _channels, float _sample_rate);
    virtual SoLoud::AudioSourceInstance* createInstance();

    uint32_t samples{};
    uint32_t loop_start{};
    std::streampos data_begin{};
    std::ifstream file;
    static constexpr std::array<int32_t, 5> filter0{ 0x0000, 0x00F0, 0x01CC, 0x0188, 0x01E8 };
    static constexpr std::array<int32_t, 5> filter1{ 0x0000, 0x0000, -0x00D0, -0x00DC, -0x00F0 };
    std::vector<int> decoder_state;
    uint32_t current_block{ 0 };
    std::vector<int> decoder_state_loop_start;
    uint32_t block_size{};

    std::vector<float> getNextBlock();
    void resetLoop();
};
