#pragma once

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
        WorkerTask<> InitWork();

        virtual Task<> drawFrame() override;
        virtual void populateAccelerationStructure(TopLevelAccelerationStructure*, BottomLevelAccelerationStructure*, const glm::mat3x4&, uint32_t, uint32_t, uint32_t) override;

        virtual void initEntity(EntityInitializer*) override;
        virtual void drawEntity(EntityInitializer*, vk::CommandBuffer, uint32_t) override;
        virtual void drawEntityShadowmap(EntityInitializer*, vk::CommandBuffer, uint32_t) {}
        virtual void initModel(RenderableEntityInitializer*, Model& model, ModelTransformedGeometry& model_transform) override;

        vk::UniqueDescriptorSetLayout static_descriptor_set_layout;
        vk::UniqueDescriptorSetLayout deferred_descriptor_set_layout;

        std::unique_ptr<Image> depth_image;
        vk::UniqueImageView depth_image_view;
        std::vector<vk::UniqueCommandBuffer> render_commandbuffers;
        vk::UniqueSemaphore raytrace_sem;

        virtual vk::Pipeline createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info) { return {}; }
        virtual vk::Pipeline createShadowmapPipeline(vk::GraphicsPipelineCreateInfo& info) { return {}; }

        virtual void bindResources(uint32_t image, std::span<vk::WriteDescriptorSet>) override;

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
            FramebufferAttachment particle;
            FramebufferAttachment motion_vector;

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

        virtual Task<> recreateRenderer() override;

        void initializeCameraBuffers();
        void generateCommandBuffers();

        virtual vk::CommandBuffer getRenderCommandbuffer(uint32_t image_index) override;
        vk::UniqueCommandBuffer getDeferredCommandBuffer(uint32_t image_index);
    };
}
