#pragma once
#include <fstream>
#include <array>
#include <vector>
#define SOLOUD_NO_ASSERTS
#include <soloud.h>
#include "engine/types.h"

class PCM;

class PCMInstance : public SoLoud::AudioSourceInstance
{
public:
    PCMInstance(PCM*);
    virtual unsigned int getAudio(float* buffer, unsigned int samples, unsigned int buffer_size);
    virtual bool hasEnded();
private:
    size_t offset{ 0 };
    PCM* pcm;
};

class PCM : public SoLoud::AudioSource
{
public:
    PCM(std::ifstream&& file, uint32_t samples, uint32_t block_size, uint32_t loop_start, uint32_t channels, float sample_rate);
    virtual SoLoud::AudioSourceInstance* createInstance();

    uint32_t samples{};
    uint32_t loop_start{};
    std::vector<std::vector<float>> data;
};
