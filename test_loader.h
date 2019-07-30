#pragma once
#include "core/core.h"
#include "core/renderer/mesh.h"
#include "core/renderer/texture.h"
#include "core/task/model_init.h"
#include "core/task/texture_init.h"
#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

class TestLoader : public lotus::MeshLoader
{
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static std::vector<vk::VertexInputBindingDescription> getBindingDescriptions() {
        std::vector<vk::VertexInputBindingDescription> binding_descriptions(1);

        binding_descriptions[0].binding = 0;
        binding_descriptions[0].stride = sizeof(Vertex);
        binding_descriptions[0].inputRate = vk::VertexInputRate::eVertex;

        return binding_descriptions;
    }

    static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(3);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
    };

    const std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4
    };

public:
    virtual std::unique_ptr<lotus::WorkItem> LoadMesh(std::shared_ptr<lotus::Mesh>& model) override
    {
        model->setVertexInputAttributeDescription(Vertex::getAttributeDescriptions());
        model->setVertexInputBindingDescription(Vertex::getBindingDescriptions());
        model->setIndexCount(static_cast<int>(indices.size()));

        std::vector<uint8_t> vertices_uint8;
        vertices_uint8.resize(vertices.size() * sizeof(Vertex));
        memcpy(vertices_uint8.data(), vertices.data(), vertices_uint8.size());

        std::vector<uint8_t> indices_uint8;
        indices_uint8.resize(indices.size() * sizeof(uint16_t));
        memcpy(indices_uint8.data(), indices.data(), indices_uint8.size());

        model->vertex_buffer = engine->renderer.memory_manager->GetBuffer(vertices_uint8.size(), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
        model->index_buffer = engine->renderer.memory_manager->GetBuffer(indices_uint8.size(), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);

        return std::make_unique<lotus::ModelInitTask>(engine->renderer.getCurrentImage(), model, std::move(vertices_uint8), std::move(indices_uint8));
    }
};

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
        image_view_info.image = *texture->image->image;
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