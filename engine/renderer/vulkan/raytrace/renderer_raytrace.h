#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <glm/glm.hpp>
#include "engine/renderer/vulkan/renderer_raytrace_base.h"

namespace lotus
{
    class Engine;

    class RendererRaytrace : public RendererRaytraceBase
    {
    public:
        RendererRaytrace(Engine* engine);
        ~RendererRaytrace();

        virtual Task<> Init() override;

        virtual Task<> drawFrame() override;
        virtual void populateAccelerationStructure(TopLevelAccelerationStructure*, BottomLevelAccelerationStructure*, const glm::mat3x4&, uint64_t, uint32_t, uint32_t) override;

        virtual void initEntity(EntityInitializer*, Engine*) override;
        virtual void drawEntity(EntityInitializer*, Engine*) override;

        vk::UniqueDescriptorSetLayout static_descriptor_set_layout;
        vk::UniqueDescriptorSetLayout deferred_descriptor_set_layout;

        std::unique_ptr<Image> depth_image;
        vk::UniqueImageView depth_image_view;
        std::vector<vk::UniqueCommandBuffer> render_commandbuffers;
        vk::UniqueSemaphore raytrace_sem;

        virtual vk::Pipeline createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info) { return {}; }
        virtual vk::Pipeline createShadowmapPipeline(vk::GraphicsPipelineCreateInfo& info) { return {}; }

        virtual void bindResources(uint32_t image, vk::WriteDescriptorSet vertex, vk::WriteDescriptorSet index,
            vk::WriteDescriptorSet material, vk::WriteDescriptorSet texture, vk::WriteDescriptorSet mesh_info) override;

        struct
        {
            std::unique_ptr<Buffer> view_proj_ubo;
            uint8_t* view_proj_mapped{ nullptr };
        } camera_buffers;

        /* Ray tracing */
        vk::UniqueDescriptorSetLayout rtx_descriptor_layout_dynamic;
        vk::UniqueDescriptorSetLayout rtx_descriptor_layout_deferred;
        vk::UniquePipelineLayout rtx_pipeline_layout;
        vk::UniquePipeline rtx_pipeline;
        vk::UniqueRenderPass rtx_render_pass;
        vk::UniquePipelineLayout rtx_deferred_pipeline_layout;
        vk::UniquePipeline rtx_deferred_pipeline;

        struct RaytraceGBuffer
        {
            FramebufferAttachment albedo;
            FramebufferAttachment light;
            FramebufferAttachment normal;
            FramebufferAttachment light_post;

            vk::UniqueSampler sampler;
        } rtx_gbuffer;

    private:
        void createRayTracingResources();
        void createRenderpasses();
        void createDescriptorSetLayout();
        void createGraphicsPipeline();
        void createDepthImage();
        void createFramebuffers();
        void createSyncs();
        void createGBufferResources();
        void createDeferredCommandBuffer();
        void createAnimationResources();
        void createPostProcessingResources();

        Task<> resizeRenderer();
        Task<> recreateRenderer();

        void initializeCameraBuffers();
        void generateCommandBuffers();

        virtual vk::CommandBuffer getRenderCommandbuffer(uint32_t image_index) override;
    };
}
