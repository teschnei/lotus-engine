#pragma once

#include "engine/types.h"
#include "engine/renderer/texture.h"

namespace FFXI
{
    class DXT3
    {
    public:
        explicit DXT3(uint8_t* buffer);

        std::string name;
        uint32_t width {0};
        uint32_t height {0};
        std::vector<uint8_t> pixels;
        vk::Format format{};
    };

    class DXT3Loader : public lotus::TextureLoader
    {
    public:
        DXT3Loader(DXT3* _dxt3) : lotus::TextureLoader(), dxt3(_dxt3) {}
        virtual std::unique_ptr<lotus::WorkItem> LoadTexture(std::shared_ptr<lotus::Texture>& texture) override;
        
        DXT3* dxt3;
    };
}
