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

        virtual vk::Pipeline createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info);
        virtual vk::Pipeline createParticlePipeline(vk::GraphicsPipelineCreateInfo& info);
        virtual vk::Pipeline createShadowmapPipeline(vk::GraphicsPipelineCreateInfo& info) { return {}; }

        std::unique_ptr<Image> depth_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> depth_image_view;

        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> descriptor_layout_deferred;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> deferred_pipeline_layout;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> deferred_pipeline;

        struct RaytraceGBuffer
        {
            FramebufferAttachment colour;

            vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> sampler;
        } rtx_gbuffer;

    private:
        void createDescriptorSetLayout();
        void createRaytracingPipeline();
        void createGraphicsPipeline();
        void createDepthImage();
        void createSyncs();
        void createGBufferResources();

        virtual Task<> recreateRenderer() override;

        void initializeCameraBuffers();

        std::vector<vk::CommandBuffer> getRenderCommandbuffers();
        vk::UniqueCommandBuffer getDeferredCommandBuffer();
    };
}
