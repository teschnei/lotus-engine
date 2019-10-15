#include "dxt3.h"
#include "engine/core.h"
#include "engine/task/texture_init.h"

namespace FFXI
{
#pragma pack(push,1)
    typedef struct
    {
        uint8_t flg;
        char id[16];
        uint32_t dwnazo1;
        long  imgx, imgy;
        uint32_t dwnazo2[6];
        uint32_t widthbyte;
        char ddsType[4];
        unsigned int size;
        unsigned int noBlock;
    } IMGINFOA1;

    typedef struct
    {
        glm::u8  flg;
        char id[16];
        glm::u32 dwnazo1;			//nazo = unknown
        long  imgx, imgy;
        glm::u32 dwnazo2[6];
        glm::u32 widthbyte;
        glm::u32 unk;				//B1-extra unk, 01-no unk
        glm::u32 palet[0x100];
    } IMGINFOB1;
#pragma pack(pop)

    DXT3::DXT3(uint8_t* buffer)
    {
        IMGINFOA1* infoa1 = reinterpret_cast<IMGINFOA1*>(buffer);

        switch (infoa1->flg)
        {
            case 0xA1:
            {
                name = std::string(infoa1->id, 16);
                width = infoa1->imgx;
                height = infoa1->imgy;
                pixels.resize(infoa1->size);
                format = vk::Format::eBc2UnormBlock;
                memcpy(pixels.data(), buffer + sizeof(IMGINFOA1), infoa1->size);
                break;
            }
            case 0x01:
                __debugbreak();
                break;
            case 0x81:
                __debugbreak();
                break;
            case 0xB1:
            {
                IMGINFOB1* infob1 = reinterpret_cast<IMGINFOB1*>(buffer);
                name = std::string(infob1->id, 16);
                width = infob1->imgx;
                height = infob1->imgy;
                format = vk::Format::eR8G8B8A8Unorm;
                uint32_t size = width * height;
                pixels.resize(static_cast<uint64_t>(size) * 4);
                uint8_t* buf_p = buffer + sizeof(IMGINFOB1);
                for (int i = height-1; i>=0; --i)
                {
                    for (uint32_t j = 0; j < width; ++j)
                    {
                        memcpy(pixels.data() + ((i * static_cast<uint64_t>(width) + j) * 4), &infob1->palet[(size_t)(*buf_p)], 4);
                        ++buf_p;
                    }
                }
                    
                break;
            }
            case 0x05:
                __debugbreak();
                break;
            case 0x91:
                __debugbreak();
                break;
        }
    }

    void DXT3Loader::LoadTexture(std::shared_ptr<lotus::Texture>& texture) 
    {
        uint32_t stride = 4;
        if (dxt3->format == vk::Format::eBc2UnormBlock)
            stride = 1;
        VkDeviceSize imageSize = static_cast<uint64_t>(dxt3->width) * static_cast<uint64_t>(dxt3->height) * stride;

        texture->setWidth(dxt3->width);
        texture->setHeight(dxt3->height);

        if (dxt3->pixels.empty()) {
            throw std::runtime_error("failed to load texture image!");
        }

        std::vector<uint8_t> texture_data;
        texture_data.resize(imageSize);
        memcpy(texture_data.data(), dxt3->pixels.data(), imageSize);

        texture->image = engine->renderer.memory_manager->GetImage(texture->getWidth(), texture->getHeight(), dxt3->format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = texture->image->image;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = dxt3->format;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        texture->image_view = engine->renderer.device->createImageViewUnique(image_view_info, nullptr, engine->renderer.dispatch);

        vk::SamplerCreateInfo sampler_info = {};
        sampler_info.magFilter = vk::Filter::eLinear;
        sampler_info.minFilter = vk::Filter::eLinear;
        sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
        sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
        sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
        sampler_info.anisotropyEnable = true;
        sampler_info.maxAnisotropy = 16;
        sampler_info.borderColor = vk::BorderColor::eIntOpaqueBlack;
        sampler_info.unnormalizedCoordinates = false;
        sampler_info.compareEnable = false;
        sampler_info.compareOp = vk::CompareOp::eAlways;
        sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;

        texture->sampler = engine->renderer.device->createSamplerUnique(sampler_info, nullptr, engine->renderer.dispatch);

        engine->worker_pool.addWork(std::make_unique<lotus::TextureInitTask>(engine->renderer.getCurrentImage(), texture, dxt3->format, vk::ImageTiling::eOptimal, std::move(texture_data)));
    }
        
}
