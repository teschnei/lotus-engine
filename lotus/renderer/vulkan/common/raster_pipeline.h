#pragma once

#include "lotus/renderer/vulkan/vulkan_inc.h"
#include "lotus/renderer/memory.h"

namespace lotus
{
    class Renderer;

    class RasterPipeline
    {
    public:
        RasterPipeline(Renderer* renderer);

        auto getPipelineLayout() { return *pipeline_layout; }
        auto getDescriptorSetLayout() { return *descriptor_set_layout; }
        vk::PipelineRenderingCreateInfoKHR getMainRenderPassInfo();
        vk::PipelineRenderingCreateInfoKHR getTransparentRenderPassInfo();
        const auto& getGBuffer() { return gbuffer; }
        void beginRendering(vk::CommandBuffer buffer);
        void endRendering(vk::CommandBuffer buffer);
        void beginMainCommandBufferRendering(vk::CommandBuffer buffer, vk::RenderingFlagsKHR flags);
        void beginTransparencyCommandBufferRendering(vk::CommandBuffer buffer, vk::RenderingFlagsKHR flags);

    private:
        Renderer* renderer;

        struct FramebufferAttachment
        {
            std::unique_ptr<Image> image;
            vk::UniqueImageView image_view;
        };

        vk::UniqueDescriptorSetLayout descriptor_set_layout;
        vk::UniquePipelineLayout pipeline_layout;

        struct GBuffer
        {
            FramebufferAttachment position;
            FramebufferAttachment normal;
            FramebufferAttachment face_normal;
            FramebufferAttachment albedo;
            FramebufferAttachment material;
            FramebufferAttachment light_type;
            FramebufferAttachment motion_vector;
            FramebufferAttachment accumulation;
            FramebufferAttachment revealage;
            FramebufferAttachment particle;
            FramebufferAttachment depth;

            vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> sampler;
        } gbuffer;

        std::vector<vk::Format> main_attachment_formats;
        std::vector<vk::Format> transparency_attachment_formats;

        static vk::UniqueDescriptorSetLayout initializeDescriptorSetLayout(Renderer* renderer);
        static vk::UniquePipelineLayout initializePipelineLayout(Renderer* renderer, vk::DescriptorSetLayout descriptor_set_layout);
        static std::vector<vk::Format> initializeMainRenderPass(Renderer* renderer);
        static std::vector<vk::Format> initializeTransparentRenderPass(Renderer* renderer);
        static FramebufferAttachment initializeFramebufferAttachment(Renderer* renderer, vk::Extent2D extent, vk::Format format, vk::ImageUsageFlags usage_flags);
        static GBuffer initializeGBuffer(Renderer* renderer);
    };
}