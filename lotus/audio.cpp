module;

#include <memory>
// asserts bring in windows.h, which is the devil
#define SOLOUD_NO_ASSERTS
#include <soloud.h>

module lotus;

import :core.audio;

import :core.config;
import :core.engine;

namespace lotus
{
AudioEngine::AudioEngine(Engine* _engine) : soloud(std::make_unique<SoLoud::Soloud>()), engine(_engine) { soloud->init(); }

AudioEngine::AudioInstance AudioEngine::playBGM(SoLoud::AudioSource& src)
{
    auto bgm = soloud->playBackground(src, engine->config->audio.master_volume * engine->config->audio.bgm_volume);
    soloud->setProtectVoice(bgm, true);
    return AudioInstance{this, bgm};
}

AudioEngine::AudioInstance AudioEngine::playSound(SoLoud::AudioSource& src)
{
    auto se = soloud->play(src, engine->config->audio.master_volume * engine->config->audio.se_volume);
    return AudioInstance{this, se};
}

AudioEngine::AudioInstance::AudioInstance(AudioEngine* _engine, SoLoud::handle _handle) : engine(_engine), handle(_handle) {}
AudioEngine::AudioInstance::~AudioInstance()
{
    if (handle)
        engine->soloud->stop(handle);
}
} // namespace lotus
