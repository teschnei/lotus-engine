#pragma once

#include <cstdint>
#include "core/renderer/texture.h"

namespace FFXI
{
    class Texture
    {
    public:
        explicit Texture(uint8_t* buffer);

        std::string name;
        uint32_t width;
        uint32_t height;
        std::vector<uint8_t> pixels;
        vk::Format format;
    };
}
