module;

#include <SDL3/SDL.h>
#include <memory>

export module lotus:audio.engine;

import :audio.instance;
import :audio.source;

export namespace lotus
{
class Engine;
class AudioEngine
{
public:
    AudioEngine(Engine* engine);
    ~AudioEngine();
    std::unique_ptr<AudioInstance> playSound(AudioSource& source, float volume = 1.f);
    std::unique_ptr<AudioInstance> playMusic(AudioSource& source, float volume = 1.f);

private:
    std::unique_ptr<AudioInstance> play(AudioSource& source, float volume);
    Engine* engine;
    SDL_AudioDeviceID device;
};
} // namespace lotus
