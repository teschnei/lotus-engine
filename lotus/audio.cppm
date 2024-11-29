module;

#include <memory>
#define SOLOUD_NO_ASSERTS
#include <soloud.h>

export module lotus:core.audio;

export namespace lotus
{
class Engine;
class AudioEngine
{
public:
    AudioEngine(Engine* engine);

    class AudioInstance
    {
    public:
        AudioInstance(AudioEngine* _engine, SoLoud::handle _handle);
        AudioInstance(const AudioInstance&) = delete;
        AudioInstance(AudioInstance&& o) noexcept
        {
            engine = o.engine;
            handle = o.handle;
            o.handle = 0;
        }
        AudioInstance& operator=(const AudioInstance&) = delete;
        AudioInstance& operator=(AudioInstance&& o) noexcept
        {
            engine = o.engine;
            handle = o.handle;
            o.handle = 0;
            return *this;
        }
        ~AudioInstance();

    private:
        AudioEngine* engine{};
        SoLoud::handle handle{};
    };

    AudioInstance playBGM(SoLoud::AudioSource&);
    AudioInstance playSound(SoLoud::AudioSource&);

private:
    std::unique_ptr<SoLoud::Soloud> soloud;
    Engine* engine;
};
} // namespace lotus
