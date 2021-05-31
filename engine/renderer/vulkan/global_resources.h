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
        void AddResources(RenderableEntity* entity);
        void AddResources(DeformableEntity* entity);
        void AddResources(Particle* entity);

        struct MeshInfo
        {
            uint32_t vertex_index_offset;
            uint32_t texture_offset;
            uint32_t indices;
            uint32_t material_index;
            glm::vec3 scale;
            uint32_t billboard;
            glm::vec4 colour;
            glm::vec2 uv_offset;
            float animation_frame;
            float _pad;
        };
        std::unique_ptr<Buffer> mesh_info_buffer;
        MeshInfo* mesh_info_buffer_mapped{ nullptr };
        static constexpr uint16_t max_resource_index{ 4096 };
        std::mutex resource_descriptor_mutex;

        std::vector<vk::DescriptorBufferInfo> descriptor_vertex_info;
        std::vector<vk::DescriptorBufferInfo> descriptor_index_info;
        std::vector<vk::DescriptorBufferInfo> descriptor_material_info;
        std::vector<vk::DescriptorImageInfo> descriptor_texture_info;

    private:
        Engine* engine;
        uint16_t mesh_info_offset{ 0 };
        uint32_t static_binding_offset_data{ 0 };

    public:
        //externally modifying static_binding_offset_data needs to be synchronized for threads, but not internally
        std::atomic_ref<uint32_t> static_binding_offset{ static_binding_offset_data };
    };
}