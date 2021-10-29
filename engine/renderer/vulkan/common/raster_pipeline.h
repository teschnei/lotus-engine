#pragma once

#include "engine/renderer/vulkan/vulkan_inc.h"
#include "engine/renderer/memory.h"

namespace lotus
{
    class Renderer;

    class RasterPipeline
    {
    public:
        RasterPipeline(Renderer* renderer);

        auto getPipelineLayout() { return *pipeline_layout; }
        auto getDescriptorSetLayout() { return *descriptor_set_layout; }
        auto getRenderPass() { return *render_pass; }
        std::vector<vk::ClearValue> getRenderPassClearValues();
        const auto& getGBuffer() { return gbuffer; }

    private:
        Renderer* renderer;

        struct FramebufferAttachment
        {
            std::unique_ptr<Image> image;
            vk::UniqueImageView image_view;
        };

        vk::UniqueDescriptorSetLayout descriptor_set_layout;
        vk::UniquePipelineLayout pipeline_layout;
        vk::UniqueRenderPass render_pass;

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
            FramebufferAttachment depth;

            vk::UniqueHandle<vk::Framebuffer, vk::DispatchLoaderDynamic> frame_buffer;
            vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> sampler;
        } gbuffer;

        static vk::UniqueDescriptorSetLayout initializeDescriptorSetLayout(Renderer* renderer);
        static vk::UniquePipelineLayout initializePipelineLayout(Renderer* renderer, vk::DescriptorSetLayout descriptor_set_layout);
        static vk::UniqueRenderPass initializeRenderPass(Renderer* renderer, vk::PipelineLayout pipeline_layout);
        static FramebufferAttachment initializeFramebufferAttachment(Renderer* renderer, vk::Extent2D extent, vk::Format format, vk::ImageUsageFlags usage_flags);
        static GBuffer initializeGBuffer(Renderer* renderer, vk::RenderPass render_pass);
    };
}