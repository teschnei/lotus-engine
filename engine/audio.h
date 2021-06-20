#pragma once
#include <memory>
//asserts bring in windows.h, which is the devil
#define SOLOUD_NO_ASSERTS
#include <soloud.h>

namespace lotus
{
    class AudioEngine
    {
    public:
        AudioEngine();

        SoLoud::handle playBGM(SoLoud::AudioSource&);

    private:
        std::unique_ptr<SoLoud::Soloud> engine;
    };
}
