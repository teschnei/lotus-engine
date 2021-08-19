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
        virtual void populateAccelerationStructure(TopLevelAccelerationStructure*, BottomLevelAccelerationStructure*, const glm::mat3x4&, uint64_t, uint32_t, uint32_t) override {}

        virtual void initEntity(EntityInitializer*, Engine*) override;
        virtual void drawEntity(EntityInitializer*, Engine*) override;
        virtual void initModel(RenderableEntityInitializer*, Engine*, Model& model, ModelTransformedGeometry& model_transform) override;

        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> render_pass;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> shadowmap_render_pass;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> gbuffer_render_pass;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> static_descriptor_set_layout;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> shadowmap_descriptor_set_layout;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> deferred_descriptor_set_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> pipeline_layout;
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

        virtual void bindResources(uint32_t image, vk::WriteDescriptorSet vertex, vk::WriteDescriptorSet index,
            vk::WriteDescriptorSet material, vk::WriteDescriptorSet texture, vk::WriteDescriptorSet mesh_info) override;

        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> deferred_pipeline;
        std::unique_ptr<Image> depth_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> depth_image_view;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> render_commandbuffers;
        static constexpr uint32_t shadowmap_cascades {4};

        struct ShadowmapCascade
        {
            vk::UniqueHandle<vk::Framebuffer, vk::DispatchLoaderDynamic> shadowmap_frame_buffer;
            vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> shadowmap_image_view;
        };

        std::array<ShadowmapCascade, shadowmap_cascades> cascades;

        vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> shadowmap_sampler;
        std::unique_ptr<Image> shadowmap_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> shadowmap_image_view;

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
            std::unique_ptr<Buffer> cascade_data_ubo;
            uint8_t* cascade_data_mapped{ nullptr };
        } camera_buffers;

        struct UBOFS
        {
            glm::vec4 cascade_splits;
            glm::mat4 cascade_view_proj[RendererRasterization::shadowmap_cascades];
            glm::mat4 inverse_view;
        } cascade_data {};

    private:

        void createRenderpasses();
        void createDescriptorSetLayout();
        void createGraphicsPipeline();
        void createDepthImage();
        void createFramebuffers();
        void createSyncs();
        void createShadowmapResources();
        void createGBufferResources();
        void createDeferredCommandBuffer();

        virtual Task<> recreateRenderer() override;

        void initializeCameraBuffers();
        void generateCommandBuffers();

        void updateCameraBuffers();

        virtual vk::CommandBuffer getRenderCommandbuffer(uint32_t image_index) override;
    };
}
