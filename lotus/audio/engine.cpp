module;

#include <SDL3/SDL.h>
#include <iostream>
#include <memory>

module lotus;

import :audio.engine;

import :audio.instance;
import :audio.source;

namespace lotus
{
AudioEngine::AudioEngine(Engine* _engine) : engine(_engine)
{
    device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    std::cerr << SDL_GetError();
    SDL_ResumeAudioDevice(device);
}
AudioEngine::~AudioEngine() { SDL_CloseAudioDevice(device); }

std::unique_ptr<AudioInstance> AudioEngine::playSound(AudioSource& source, float in_volume)
{
    Config* config = engine->config.get();
    float volume = in_volume * config->audio.master_volume * config->audio.se_volume;
    return play(source, volume);
}

std::unique_ptr<AudioInstance> AudioEngine::playMusic(AudioSource& source, float in_volume)
{
    Config* config = engine->config.get();
    float volume = in_volume * config->audio.master_volume * config->audio.bgm_volume;
    return play(source, volume);
}

std::unique_ptr<AudioInstance> AudioEngine::play(AudioSource& source, float volume)
{
    auto instance = source.CreateInstance();
    SDL_AudioStream* sdl_stream = source.CreateSDLStream();
    bool success = SDL_SetAudioStreamGetCallback(sdl_stream, audioCallback, instance.get());
    SDL_SetAudioStreamGain(sdl_stream, volume);
    success = SDL_BindAudioStream(device, sdl_stream);
    std::cerr << SDL_GetError();
    instance->SetSDLStream(sdl_stream);
    return instance;
}
} // namespace lotus
