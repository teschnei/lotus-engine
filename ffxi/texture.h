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
        uint32_t width {0};
        uint32_t height {0};
        std::vector<uint8_t> pixels;
        vk::Format format{};
    };
}
