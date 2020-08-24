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

        virtual void Init() override;

        virtual void drawFrame() override;
        virtual void populateAccelerationStructure(TopLevelAccelerationStructure*, BottomLevelAccelerationStructure*, const glm::mat3x4&, uint64_t, uint32_t, uint32_t) override;

        virtual void initEntity(EntityInitializer*, WorkerThread*) override;
        virtual void drawEntity(EntityInitializer*, WorkerThread*) override;

        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> static_descriptor_set_layout;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> deferred_descriptor_set_layout;

        std::unique_ptr<Image> depth_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> depth_image_view;
        std::vector<vk::UniqueHandle<vk::Framebuffer, vk::DispatchLoaderDynamic>> frame_buffers;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> render_commandbuffers;
        vk::UniqueHandle<vk::Semaphore, vk::DispatchLoaderDynamic> raytrace_sem;

        struct
        {
            std::unique_ptr<Buffer> view_proj_ubo;
            uint8_t* view_proj_mapped{ nullptr };
        } camera_buffers;

        ///* Ray tracing */
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
        void createRenderpasses();
        void createDescriptorSetLayout();
        void createGraphicsPipeline();
        void createDepthImage();
        void createFramebuffers();
        void createSyncs();
        void createCommandPool();
        void createGBufferResources();
        void createDeferredCommandBuffer();
        void createQuad();
        void createAnimationResources();

        void resizeRenderer();
        void recreateRenderer();
        void recreateStaticCommandBuffers();

        void initializeCameraBuffers();
        void generateCommandBuffers();

        vk::CommandBuffer getRenderCommandbuffer(uint32_t image_index);
    };
}
