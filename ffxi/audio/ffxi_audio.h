#pragma once

#include <filesystem>

namespace SoLoud
{
    class AudioSource;
}

namespace FFXI
{
    class Audio
    {
    public:
        //TODO: change to ID
        static std::unique_ptr<SoLoud::AudioSource> loadSound(std::filesystem::path path);
    };
}
