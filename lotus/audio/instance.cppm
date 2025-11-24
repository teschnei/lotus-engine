module;

#include <SDL3/SDL.h>
#include <vector>

export module lotus:audio.instance;

import :audio.source;

export namespace lotus
{
class AudioInstance
{
public:
    enum Flags : uint8_t
    {
        Looping,
    };

    AudioInstance(const AudioInstance&) = delete;
    AudioInstance& operator=(const AudioInstance&) = delete;
    AudioInstance(AudioInstance&&) = default;
    AudioInstance& operator=(AudioInstance&&) = default;
    virtual ~AudioInstance()
    {
        if (stream)
            SDL_DestroyAudioStream(stream);
    }

    SDL_AudioStream* GetSDLStream() { return stream; }
    void SetSDLStream(SDL_AudioStream* _stream) { stream = _stream; }
    bool IsLooping() { return flags & Flags::Looping; }

    virtual unsigned int getAudio(float* buffer, unsigned int samples) = 0;
    virtual bool hasEnded() = 0;

protected:
    AudioInstance(AudioSource* source) : channels(source->GetChannels()), frequency(source->GetFrequency()) {}

    uint8_t flags;
    int channels;
    int frequency;

private:
    SDL_AudioStream* stream{nullptr};
};

void audioCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount)
{
    AudioInstance* instance = reinterpret_cast<AudioInstance*>(userdata);
    int samples = additional_amount / sizeof(float);
    std::vector<float> buffer(samples);
    int total_samples = 0;
    while (total_samples < samples && (instance->IsLooping() || !instance->hasEnded()))
    {
        total_samples += instance->getAudio(buffer.data() + total_samples, samples - total_samples);
    }
    SDL_PutAudioStreamData(instance->GetSDLStream(), buffer.data(), total_samples * sizeof(float));
    if (instance->hasEnded())
    {
        SDL_UnbindAudioStream(instance->GetSDLStream());
    }
}
} // namespace lotus
