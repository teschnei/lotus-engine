#pragma once

#include <glm/glm.hpp>
#include <span>
#include <vector>
#include "engine/renderer/vulkan/vulkan_inc.h"
#include "engine/renderer/vulkan/descriptor_resource.h"
#include "engine/renderer/vulkan/buffer_resource.h"

namespace lotus
{
    class Renderer;
    class GlobalDescriptors
    {
    public:
        struct MeshInfo
        {
            uint32_t vertex_offset;
            uint32_t index_offset;
            uint32_t indices;
            uint32_t material_index;
            glm::vec3 scale;
            uint32_t billboard;
            glm::vec4 colour;
            glm::vec2 uv_offset;
            float animation_frame;
            uint32_t vertex_prev_offset;
            glm::mat4x4 model_prev;
        };

        GlobalDescriptors(Renderer*);

        using VertexDescriptor = DescriptorResource<vk::DescriptorType::eStorageBuffer, 0>;
        using IndexDescriptor = DescriptorResource<vk::DescriptorType::eStorageBuffer, 1>;
        using TextureDescriptor = DescriptorResource<vk::DescriptorType::eCombinedImageSampler, 2>;
        using MaterialDescriptor = DescriptorResource<vk::DescriptorType::eUniformBuffer, 3>;
        using MeshInfoBuffer = BufferResource<MeshInfo, 4>;

        static constexpr size_t max_descriptor_index{ 4096 };

        std::unique_ptr<VertexDescriptor::Index> getVertexIndex();
        std::unique_ptr<IndexDescriptor::Index> getIndexIndex();
        std::unique_ptr<TextureDescriptor::Index> getTextureIndex();
        std::unique_ptr<MaterialDescriptor::Index> getMaterialIndex();
        std::unique_ptr<MeshInfoBuffer::View> getMeshInfoBuffer(uint32_t count);

        void updateDescriptorSet();
        vk::DescriptorSetLayout getDescriptorLayout() { return *layout; }
        vk::DescriptorSet getDescriptorSet() { return *set; }
    private:
        Renderer* renderer;

        vk::UniqueDescriptorSetLayout layout;
        vk::UniqueDescriptorPool pool;
        vk::UniqueDescriptorSet set;

        VertexDescriptor vertex;
        IndexDescriptor index;
        TextureDescriptor texture;
        MaterialDescriptor material;
        MeshInfoBuffer mesh_info;

        static vk::UniqueDescriptorSetLayout initializeResourceDescriptorSetLayout(Renderer* renderer);
        static vk::UniqueDescriptorPool initializeResourceDescriptorPool(Renderer* renderer, vk::DescriptorSetLayout layout);
        static vk::UniqueDescriptorSet initializeResourceDescriptorSet(Renderer* renderer, vk::DescriptorSetLayout layout, vk::DescriptorPool pool);
    };
}