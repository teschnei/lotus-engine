#pragma once

#include "dat_chunk.h"
#include "engine/audio.h"

namespace FFXI
{
    class Audio;
    class Sep : public DatChunk
    {
    public:
        Sep(char* name, uint8_t* buffer, size_t len);
        std::optional<lotus::AudioEngine::AudioInstance> playSound(FFXI::Audio* audio);
    };
}
