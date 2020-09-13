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

        vk::CommandBuffer Render(int image_index);
        void GenerateRenderBuffers(std::shared_ptr<ui::Element>);

    private:

        void createDescriptorSetLayout();
        void createRenderpass();
        void createPipeline();
        void createBuffers();

        struct Quad
        {
            std::unique_ptr<Buffer> vertex_buffer;
        } quad;

        Engine* engine;
        Renderer* renderer;
        vk::UniquePipelineLayout pipeline_layout;
        vk::UniquePipeline pipeline;
        vk::UniqueRenderPass renderpass;
        vk::UniqueDescriptorSetLayout descriptor_set_layout;
        std::vector<vk::UniqueCommandBuffer> command_buffers;

        std::shared_ptr<Texture> default_texture;
    };
}

