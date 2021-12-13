#pragma once

#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    class Engine;

    class RendererRaytrace : public Renderer
    {
    public:
        RendererRaytrace(Engine* engine);
        ~RendererRaytrace();

        virtual Task<> Init() override;
        WorkerTask<> InitWork();

        virtual Task<> drawFrame() override;

        vk::UniqueDescriptorSetLayout static_descriptor_set_layout;
        vk::UniqueDescriptorSetLayout deferred_descriptor_set_layout;

        std::unique_ptr<Image> depth_image;
        vk::UniqueImageView depth_image_view;
        std::vector<vk::UniqueCommandBuffer> render_commandbuffers;

        virtual vk::Pipeline createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info) { return {}; }
        virtual vk::Pipeline createShadowmapPipeline(vk::GraphicsPipelineCreateInfo& info) { return {}; }

        /* Ray tracing */
        vk::UniqueDescriptorSetLayout rtx_descriptor_layout_deferred;
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

        vk::CommandBuffer getRenderCommandbuffer();
        vk::UniqueCommandBuffer getDeferredCommandBuffer();
    };
}
