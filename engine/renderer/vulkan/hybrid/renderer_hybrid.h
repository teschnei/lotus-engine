#pragma once

#include "engine/renderer/vulkan/renderer_raytrace_base.h"

namespace lotus
{
    class Engine;

    class RendererHybrid : public RendererRaytraceBase
    {
    public:
        RendererHybrid(Engine* engine);
        ~RendererHybrid();

        virtual Task<> Init() override;
        WorkerTask<> InitWork();

        virtual Task<> drawFrame() override;
        virtual void populateAccelerationStructure(TopLevelAccelerationStructure*, BottomLevelAccelerationStructure*, const glm::mat3x4&, uint32_t, uint32_t, uint32_t) override;

        virtual void initEntity(EntityInitializer*) override;
        virtual void drawEntity(EntityInitializer*, vk::CommandBuffer, uint32_t) override;
        virtual void drawEntityShadowmap(EntityInitializer*, vk::CommandBuffer, uint32_t) {}
        virtual void initModel(RenderableEntityInitializer*, Model& model, ModelTransformedGeometry& model_transform) override;

        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> render_pass;

        virtual vk::Pipeline createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info);
        virtual vk::Pipeline createShadowmapPipeline(vk::GraphicsPipelineCreateInfo& info) { return {}; }

        virtual void bindResources(uint32_t image, std::span<vk::WriteDescriptorSet>) override;

        std::unique_ptr<Image> depth_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> depth_image_view;

        vk::UniqueHandle<vk::Semaphore, vk::DispatchLoaderDynamic> gbuffer_sem;

        /* Ray tracing */
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> rtx_descriptor_layout_deferred;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> rtx_render_pass;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> rtx_deferred_pipeline_layout;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> rtx_deferred_pipeline;
        vk::UniqueDescriptorPool descriptor_pool_deferred;
        std::vector<vk::UniqueDescriptorSet> descriptor_set_deferred;

        struct RaytraceGBuffer
        {
            FramebufferAttachment colour;

            vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> sampler;
        } rtx_gbuffer;

    private:
        void createRenderpasses();
        void createDescriptorSetLayout();
        void createRaytracingPipeline();
        void createGraphicsPipeline();
        void createDepthImage();
        void createFramebuffers();
        void createSyncs();
        void createGBufferResources();

        virtual Task<> recreateRenderer() override;

        void initializeCameraBuffers();
        void generateCommandBuffers();

        std::pair<vk::UniqueCommandBuffer, vk::UniqueCommandBuffer> getRenderCommandbuffers(uint32_t image_index);
        vk::UniqueCommandBuffer getDeferredCommandBuffer(uint32_t image_index);
    };
}
