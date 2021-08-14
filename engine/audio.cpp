#include "audio.h"

namespace lotus
{
    AudioEngine::AudioEngine() : engine(std::make_unique<SoLoud::Soloud>())
    {
        engine->init();
    }

    SoLoud::handle AudioEngine::playBGM(SoLoud::AudioSource& src)
    {
        auto bgm = engine->playBackground(src);
        engine->setProtectVoice(bgm, true);
        return bgm;
    }

    SoLoud::handle AudioEngine::playSound(SoLoud::AudioSource& src)
    {
        auto se = engine->play(src);
        return se;
    }
}