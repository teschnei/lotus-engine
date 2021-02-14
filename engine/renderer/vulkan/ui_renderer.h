#pragma once

#include "vulkan_inc.h"
#include "engine/renderer/memory.h"
#include "engine/renderer/texture.h"
#include "engine/ui/element.h"

namespace lotus
{
    class Engine;
    class Renderer;
    class UiRenderer
    {
    public:
        UiRenderer(Engine*, Renderer*);
        Task<> Init();
        Task<> ReInit();

        vk::CommandBuffer Render(int image_index);
        void GenerateRenderBuffers(ui::Element*);

    private:

        void createDescriptorSetLayout();
        void createRenderpass();
        void createDepthImage();
        void createFrameBuffers();
        void createPipeline();
        Task<> createBuffers();

        struct Quad
        {
            std::unique_ptr<Buffer> vertex_buffer;
        } quad;

        Engine* engine;
        Renderer* renderer;
        vk::UniquePipelineLayout pipeline_layout;
        vk::UniquePipeline pipeline;
        vk::UniqueRenderPass renderpass;
        std::vector<vk::UniqueFramebuffer> framebuffers;
        vk::UniqueDescriptorSetLayout descriptor_set_layout;
        std::vector<vk::UniqueCommandBuffer> command_buffers;

        std::unique_ptr<Image> depth_image;
        vk::UniqueImageView depth_image_view;

        std::shared_ptr<Texture> default_texture;
    };
}

