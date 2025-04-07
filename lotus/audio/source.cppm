module;

#include <SDL3/SDL.h>
#include <memory>

export module lotus:audio.source;

export namespace lotus
{
class AudioInstance;
class AudioSource
{
public:
    virtual ~AudioSource() {}
    virtual std::unique_ptr<AudioInstance> CreateInstance() = 0;
    SDL_AudioStream* CreateSDLStream()
    {
        SDL_AudioSpec spec = {.format = SDL_AUDIO_F32, .channels = channels, .freq = frequency};
        return SDL_CreateAudioStream(&spec, &spec);
    }

    int GetChannels() { return channels; }
    int GetFrequency() { return frequency; }

protected:
    AudioSource(int _channels, int _frequency) : channels(_channels), frequency(_frequency) {}

    const int channels;
    const int frequency;
};
} // namespace lotus
