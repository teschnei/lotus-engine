#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <glm/glm.hpp>
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
        virtual void populateAccelerationStructure(TopLevelAccelerationStructure*, BottomLevelAccelerationStructure*, const glm::mat3x4&, uint64_t, uint32_t, uint32_t) override;

        virtual void initEntity(EntityInitializer*, Engine*) override;
        virtual void drawEntity(EntityInitializer*, Engine*) override;

        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> render_pass;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> gbuffer_render_pass;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> static_descriptor_set_layout;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> deferred_descriptor_set_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> pipeline_layout;

        struct PipelineGroup
        {
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> graphics_pipeline;
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> blended_graphics_pipeline;
        } landscape_pipeline_group, main_pipeline_group, particle_pipeline_group;

        std::unique_ptr<Image> depth_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> depth_image_view;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> render_commandbuffers;

        struct GBuffer
        {
            FramebufferAttachment position;
            FramebufferAttachment normal;
            FramebufferAttachment face_normal;
            FramebufferAttachment albedo;
            FramebufferAttachment accumulation;
            FramebufferAttachment revealage;
            FramebufferAttachment material;
            FramebufferAttachment depth;

            vk::UniqueHandle<vk::Framebuffer, vk::DispatchLoaderDynamic> frame_buffer;
            vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> sampler;
        } gbuffer;

        vk::UniqueHandle<vk::Semaphore, vk::DispatchLoaderDynamic> gbuffer_sem;

        struct
        {
            std::unique_ptr<Buffer> view_proj_ubo;
            uint8_t* view_proj_mapped{ nullptr };
        } camera_buffers;

        /* Ray tracing */
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> rtx_descriptor_layout_dynamic;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> rtx_descriptor_layout_deferred;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> rtx_pipeline_layout;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> rtx_pipeline;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> rtx_render_pass;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> rtx_deferred_pipeline_layout;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> rtx_deferred_pipeline;

        struct RaytraceGBuffer
        {
            FramebufferAttachment albedo;
            FramebufferAttachment light;

            vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> sampler;
        } rtx_gbuffer;

    private:
        void createRayTracingResources();
        /* Ray tracing */

    private:
        void createRenderpasses();
        void createDescriptorSetLayout();
        void createGraphicsPipeline();
        void createDepthImage();
        void createFramebuffers();
        void createSyncs();
        void createGBufferResources();
        void createDeferredCommandBuffer();

        void initializeCameraBuffers();
        void generateCommandBuffers();

        virtual vk::CommandBuffer getRenderCommandbuffer(uint32_t image_index) override;
    };
}
