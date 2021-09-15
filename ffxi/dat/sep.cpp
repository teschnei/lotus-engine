#include "sep.h"
#include "audio/ffxi_audio.h"

namespace FFXI
{
    Sep::Sep(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
    {
    }

    std::optional<lotus::AudioEngine::AudioInstance> Sep::playSound(FFXI::Audio* audio)
    {
        uint32_t id = *(uint32_t*)(buffer + 8);
        return audio->playSound(id);
    }
}
