#pragma once

#include "renderer.h"

namespace lotus
{
    class RendererRaytraceBase : public Renderer
    {
    public:
        RendererRaytraceBase(Engine* engine) : Renderer(engine) {}

//        struct shader_binding
//        {
//            uint32_t geometry_instance;
//        };
        vk::StridedBufferRegionKHR raygenSBT;
        vk::StridedBufferRegionKHR missSBT;
        vk::StridedBufferRegionKHR hitSBT;
        std::unique_ptr<Buffer> shader_binding_table;
        static constexpr uint32_t shaders_per_group{ 1 };

        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> rtx_descriptor_layout_const;
        vk::UniqueHandle<vk::DescriptorPool, vk::DispatchLoaderDynamic> rtx_descriptor_pool_const;
        std::vector<vk::UniqueHandle<vk::DescriptorSet, vk::DispatchLoaderDynamic>> rtx_descriptor_sets_const;
    };
}
