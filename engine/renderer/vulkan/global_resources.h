#pragma once

#include <glm/glm.hpp>
#include "engine/renderer/vulkan/vulkan_inc.h"
#include "engine/renderer/memory.h"

namespace lotus
{
    class Engine;
    class Model;
    class DeformableEntity;
    class Particle;
    class Renderer;
    class RenderableEntity;

    class GlobalResources
    {
    public:
        GlobalResources(Engine* engine, Renderer* renderer);
        ~GlobalResources();
        void BindResources(uint32_t image);
        void Reset();

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
        std::unique_ptr<Buffer> mesh_info_buffer;
        MeshInfo* mesh_info_buffer_mapped{ nullptr };
        static constexpr uint16_t max_resource_index{ 4096 };
        std::mutex resource_descriptor_mutex;

        uint16_t pushVertexInfo(std::span<vk::DescriptorBufferInfo>);
        uint16_t pushVertexPrevInfo(std::span<vk::DescriptorBufferInfo>);
        uint16_t pushIndexInfo(std::span<vk::DescriptorBufferInfo>);
        uint16_t pushMaterialTextureInfo(std::span<vk::DescriptorBufferInfo>, std::span<vk::DescriptorImageInfo>);
        uint16_t pushMeshInfo(std::span<MeshInfo>);

        std::span<vk::DescriptorBufferInfo> getMaterialInfo()
        {
            return std::span{ descriptor_material_info.begin(), descriptor_material_texture_count };
        }

    private:
        Engine* engine;

        std::array<vk::DescriptorBufferInfo, max_resource_index> descriptor_vertex_info;
        std::array<vk::DescriptorBufferInfo, max_resource_index> descriptor_vertex_prev_info;
        std::array<vk::DescriptorBufferInfo, max_resource_index> descriptor_index_info;
        std::array<vk::DescriptorBufferInfo, max_resource_index> descriptor_material_info;
        std::array<vk::DescriptorImageInfo, max_resource_index> descriptor_texture_info;

        std::atomic<uint16_t> descriptor_vertex_count{ 0 };
        std::atomic<uint16_t> descriptor_vertex_prev_count{ 0 };
        std::atomic<uint16_t> descriptor_index_count{ 0 };
        std::atomic<uint16_t> descriptor_material_texture_count{ 0 };
        std::atomic<uint16_t> mesh_info_offset{ 0 };

        uint32_t static_binding_offset_data{ 0 };

    public:
        //externally modifying static_binding_offset_data needs to be synchronized for threads, but not internally
        std::atomic_ref<uint32_t> static_binding_offset{ static_binding_offset_data };
    };
}