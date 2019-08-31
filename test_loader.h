#pragma once
#include "core/core.h"
#include "core/renderer/mesh.h"
#include "core/renderer/texture.h"
#include "core/task/model_init.h"
#include "core/task/texture_init.h"
#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

class TestTextureLoader : public lotus::TextureLoader
{
public:
    virtual std::unique_ptr<lotus::WorkItem> LoadTexture(std::shared_ptr<lotus::Texture>& texture) override
    {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("textures/texture.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        texture->setWidth(texWidth);
        texture->setHeight(texHeight);

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        std::vector<uint8_t> texture_data;
        texture_data.resize(imageSize);
        memcpy(texture_data.data(), pixels, imageSize);
        stbi_image_free(pixels);

        texture->image = engine->renderer.memory_manager->GetImage(texture->getWidth(), texture->getHeight(), vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = texture->image->image;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = vk::Format::eR8G8B8A8Unorm;
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

        return std::make_unique<lotus::TextureInitTask>(engine->renderer.getCurrentImage(), texture, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, std::move(texture_data));
    }
};