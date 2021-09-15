#include "audio.h"

namespace lotus
{
    AudioEngine::AudioEngine() : engine(std::make_unique<SoLoud::Soloud>())
    {
        engine->init();
    }

    AudioEngine::AudioInstance AudioEngine::playBGM(SoLoud::AudioSource& src)
    {
        auto bgm = engine->playBackground(src);
        engine->setProtectVoice(bgm, true);
        return AudioInstance{ this, bgm };
    }

    AudioEngine::AudioInstance AudioEngine::playSound(SoLoud::AudioSource& src)
    {
        auto se = engine->play(src);
        return AudioInstance{ this, se };
    }

    AudioEngine::AudioInstance::AudioInstance(AudioEngine* _engine, SoLoud::handle _handle) : engine(_engine), handle(_handle) {}
    AudioEngine::AudioInstance::~AudioInstance()
    {
        if (handle)
            engine->engine->stop(handle);
    }
}