#pragma once

#include "dat_chunk.h"
#include "engine/renderer/texture.h"

namespace FFXI
{
    class DXT3 : public DatChunk
    {
    public:
        DXT3(char* _name, uint8_t* _buffer, size_t _len);

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
        virtual std::vector<std::unique_ptr<lotus::WorkItem>> LoadTexture(std::shared_ptr<lotus::Texture>& texture) override;
        
        DXT3* dxt3;
    };
}
