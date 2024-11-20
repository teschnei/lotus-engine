#pragma once

#include <glm/glm.hpp>
#include "lotus/renderer/vulkan/vulkan_inc.h"
#include "lotus/renderer/vulkan/descriptor_resource.h"
#include "lotus/renderer/vulkan/buffer_resource.h"

namespace lotus
{
    class Renderer;
    class GlobalDescriptors
    {
    public:
        struct MeshInfo
        {
            uint64_t vertex_buffer;
            uint64_t vertex_prev_buffer;
            uint64_t index_buffer;
            uint64_t material;
            glm::vec3 scale;
            uint32_t billboard;
            glm::vec4 colour;
            glm::vec2 uv_offset;
            float animation_frame;
            uint32_t index_count;
            glm::mat4x4 model_prev;
        };

        GlobalDescriptors(Renderer*);

        using MeshInfoBuffer = BufferResource<MeshInfo, 0>;
        using TextureDescriptor = DescriptorResource<vk::DescriptorType::eCombinedImageSampler, 1>;

        static constexpr size_t max_descriptor_index{ 4096 };

        std::unique_ptr<MeshInfoBuffer::View> getMeshInfoBuffer(uint32_t count);
        std::unique_ptr<TextureDescriptor::Index> getTextureIndex();

        void updateDescriptorSet();
        vk::DescriptorSetLayout getDescriptorLayout() { return *layout; }
        vk::DescriptorSet getDescriptorSet() { return *set; }
    private:
        Renderer* renderer;

        vk::UniqueDescriptorSetLayout layout;
        vk::UniqueDescriptorPool pool;
        vk::UniqueDescriptorSet set;

        MeshInfoBuffer mesh_info;
        TextureDescriptor texture;

        static vk::UniqueDescriptorSetLayout initializeResourceDescriptorSetLayout(Renderer* renderer);
        static vk::UniqueDescriptorPool initializeResourceDescriptorPool(Renderer* renderer, vk::DescriptorSetLayout layout);
        static vk::UniqueDescriptorSet initializeResourceDescriptorSet(Renderer* renderer, vk::DescriptorSetLayout layout, vk::DescriptorPool pool);
    };
}
