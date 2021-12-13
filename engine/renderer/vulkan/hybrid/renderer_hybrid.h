#pragma once

#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    class Engine;

    class RendererHybrid : public Renderer
    {
    public:
        RendererHybrid(Engine* engine);
        ~RendererHybrid();

        virtual Task<> Init() override;
        WorkerTask<> InitWork();

        virtual Task<> drawFrame() override;

        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> render_pass;

        virtual vk::Pipeline createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info);
        virtual vk::Pipeline createShadowmapPipeline(vk::GraphicsPipelineCreateInfo& info) { return {}; }

        std::unique_ptr<Image> depth_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> depth_image_view;

        /* Ray tracing */
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> rtx_descriptor_layout_deferred;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> rtx_render_pass;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> rtx_deferred_pipeline_layout;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> rtx_deferred_pipeline;

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

        std::pair<vk::UniqueCommandBuffer, vk::UniqueCommandBuffer> getRenderCommandbuffers();
        vk::UniqueCommandBuffer getDeferredCommandBuffer();
    };
}
