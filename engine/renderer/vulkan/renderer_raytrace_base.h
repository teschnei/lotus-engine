#pragma once

#include "renderer.h"

namespace lotus
{
    class RendererRaytraceBase : public Renderer
    {
    public:
        RendererRaytraceBase(Engine* engine) : Renderer(engine) {}

        struct MeshInfo
        {
            uint32_t vertex_index_offset;
            uint32_t texture_offset;
            float specular_exponent;
            float specular_intensity;
            glm::vec4 color;
            glm::vec3 scale;
            uint32_t billboard;
            uint32_t light_type;
            uint32_t indices;
            float _pad[2];
        };
        std::unique_ptr<Buffer> mesh_info_buffer;
        MeshInfo* mesh_info_buffer_mapped{ nullptr };

//        struct shader_binding
//        {
//            uint32_t geometry_instance;
//        };
        vk::StridedBufferRegionKHR raygenSBT;
        vk::StridedBufferRegionKHR missSBT;
        vk::StridedBufferRegionKHR hitSBT;
        std::unique_ptr<Buffer> shader_binding_table;
        static constexpr uint32_t shaders_per_group{ 1 };

        uint16_t static_acceleration_bindings_offset{ 0 };
        std::mutex acceleration_binding_mutex;
        static constexpr uint16_t max_acceleration_binding_index{ 1024 };

        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> rtx_descriptor_layout_const;
        vk::UniqueHandle<vk::DescriptorPool, vk::DispatchLoaderDynamic> rtx_descriptor_pool_const;
        std::vector<vk::UniqueHandle<vk::DescriptorSet, vk::DispatchLoaderDynamic>> rtx_descriptor_sets_const;
    };
}
