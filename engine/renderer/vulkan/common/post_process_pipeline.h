#pragma once

#include "engine/renderer/vulkan/vulkan_inc.h"
#include "engine/renderer/memory.h"

namespace lotus
{
    class Renderer;

    class PostProcessPipeline
    {
    public:
        PostProcessPipeline(Renderer* renderer);

        void Init();
        void InitWork(vk::CommandBuffer buffer);
        vk::UniqueCommandBuffer getCommandBuffer(uint32_t image_index, vk::ImageView input_colour, vk::ImageView input_normals, vk::ImageView input_motionvectors);
        vk::ImageView getOutputImageView();

    private:
        Renderer* renderer;

        struct ImageData
        {
            std::unique_ptr<Image> image;
            vk::UniqueImageView image_view;
        };

        std::array<ImageData, 2> image_buffers;
        vk::UniqueSampler history_sampler;
        std::array<ImageData, 2> factor_buffers;
        vk::UniqueSampler factor_sampler;
        uint8_t buffer_index{ 0 };

        vk::UniqueDescriptorSetLayout descriptor_set_layout;
        vk::UniquePipelineLayout pipeline_layout;
        vk::UniquePipeline pipeline;
    };
}