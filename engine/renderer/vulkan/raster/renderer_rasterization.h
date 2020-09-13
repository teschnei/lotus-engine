#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <glm/glm.hpp>
#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    class RendererRasterization : public Renderer
    {
    public:
        RendererRasterization(Engine* engine);
        ~RendererRasterization();

        virtual void Init() override;

        virtual void drawFrame() override;
        virtual void populateAccelerationStructure(TopLevelAccelerationStructure*, BottomLevelAccelerationStructure*, const glm::mat3x4&, uint64_t, uint32_t, uint32_t) override {}

        virtual void initEntity(EntityInitializer*, WorkerThread*) override;
        virtual void drawEntity(EntityInitializer*, WorkerThread*) override;

        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> render_pass;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> shadowmap_render_pass;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> gbuffer_render_pass;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> static_descriptor_set_layout;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> shadowmap_descriptor_set_layout;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> deferred_descriptor_set_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> pipeline_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> shadowmap_pipeline_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> deferred_pipeline_layout;

        struct PipelineGroup
        {
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> graphics_pipeline;
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> blended_graphics_pipeline;
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> shadowmap_pipeline;
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> blended_shadowmap_pipeline;
        } landscape_pipeline_group, main_pipeline_group, particle_pipeline_group;

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

        struct MeshInfo
        {
            uint32_t vertex_index_offset;
            uint32_t texture_offset;
            float specular_exponent;
            float specular_intensity;
            glm::vec4 color;
            glm::vec3 scale;
            uint32_t billboard;
            uint32_t light_type;
            uint32_t indices;
            float _pad[2];
        };

        //std::unique_ptr<Buffer> mesh_info_buffer;
        MeshInfo* mesh_info_buffer_mapped{ nullptr };

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

        void resizeRenderer();
        void recreateRenderer();
        void recreateStaticCommandBuffers();

        void initializeCameraBuffers();
        void generateCommandBuffers();

        void updateCameraBuffers();

        virtual vk::CommandBuffer getRenderCommandbuffer(uint32_t image_index) override;
    };
}
