#pragma once

#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    class RendererRasterization : public Renderer
    {
    public:
        RendererRasterization(Engine* engine);
        ~RendererRasterization();

        virtual Task<> Init() override;

        virtual Task<> drawFrame() override;
        virtual void populateAccelerationStructure(TopLevelAccelerationStructure*, BottomLevelAccelerationStructure*, const glm::mat3x4&, uint32_t, uint32_t, uint32_t) override {}

        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> render_pass;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> shadowmap_render_pass;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> shadowmap_descriptor_set_layout;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> deferred_descriptor_set_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> shadowmap_pipeline_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> deferred_pipeline_layout;
        vk::UniqueDescriptorPool deferred_descriptor_pool;
        std::vector<vk::UniqueDescriptorSet> deferred_descriptor_set;

        struct PipelineGroup
        {
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> graphics_pipeline;
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> blended_graphics_pipeline;
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> shadowmap_pipeline;
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> blended_shadowmap_pipeline;
        } landscape_pipeline_group, main_pipeline_group, particle_pipeline_group;

        virtual vk::Pipeline createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info);
        virtual vk::Pipeline createShadowmapPipeline(vk::GraphicsPipelineCreateInfo& info);

        virtual void bindResources(uint32_t image, std::span<vk::WriteDescriptorSet>) override {}

        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> deferred_pipeline;
        std::unique_ptr<Image> depth_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> depth_image_view;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> render_commandbuffers;

        struct ShadowmapCascade
        {
            vk::UniqueHandle<vk::Framebuffer, vk::DispatchLoaderDynamic> shadowmap_frame_buffer;
            vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> shadowmap_image_view;
        };

        std::array<ShadowmapCascade, shadowmap_cascades> cascades;

        vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> shadowmap_sampler;
        std::unique_ptr<Image> shadowmap_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> shadowmap_image_view;

        vk::UniqueHandle<vk::Semaphore, vk::DispatchLoaderDynamic> gbuffer_sem;

    private:

        void createRenderpasses();
        void createDescriptorSetLayout();
        void createGraphicsPipeline();
        void createDepthImage();
        void createFramebuffers();
        void createSyncs();
        void createShadowmapResources();

        virtual Task<> recreateRenderer() override;

        void initializeCameraBuffers();
        void generateCommandBuffers();

        void updateCameraBuffers();

        vk::CommandBuffer getRenderCommandbuffer();
        vk::UniqueCommandBuffer getDeferredCommandBuffer();
    };
}
