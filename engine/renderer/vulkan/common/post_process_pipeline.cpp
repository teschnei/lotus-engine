#include "post_process_pipeline.h"

#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    PostProcessPipeline::PostProcessPipeline(Renderer* _renderer) : renderer(_renderer) {}

    void PostProcessPipeline::Init()
    {
        //descriptor set layout
        std::array descriptors
        {
            //input colour
            vk::DescriptorSetLayoutBinding
            {
                .binding = 0,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eCompute,
                .pImmutableSamplers = nullptr,
            },
            //input normals
            vk::DescriptorSetLayoutBinding 
            {
                .binding = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eCompute,
                .pImmutableSamplers = nullptr
            },
            //input motion vectors
            vk::DescriptorSetLayoutBinding
            {
                .binding = 2,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eCompute,
                .pImmutableSamplers = nullptr
            },
            //input blend factors
            vk::DescriptorSetLayoutBinding
            {
                .binding = 3,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eCompute,
                .pImmutableSamplers = nullptr
            },
            //input history frame
            vk::DescriptorSetLayoutBinding
            {
                .binding = 4,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eCompute,
                .pImmutableSamplers = nullptr
            },
            //output blend factor
            vk::DescriptorSetLayoutBinding
            {
                .binding = 5,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eCompute,
                .pImmutableSamplers = nullptr
            },
            //output image
            vk::DescriptorSetLayoutBinding
            {
                .binding = 6,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eCompute,
                .pImmutableSamplers = nullptr
            }
        };

        vk::DescriptorSetLayoutCreateInfo layout_info
        {
            .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
            .bindingCount = descriptors.size(),
            .pBindings = descriptors.data()
        };

        descriptor_set_layout = renderer->gpu->device->createDescriptorSetLayoutUnique(layout_info, nullptr);

        std::vector<vk::DescriptorSetLayout> layouts = { *descriptor_set_layout };
        //pipeline layout
        vk::PipelineLayoutCreateInfo pipeline_layout_ci
        {
            .setLayoutCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data()
        };

        pipeline_layout = renderer->gpu->device->createPipelineLayoutUnique(pipeline_layout_ci, nullptr);

        auto post_process_module = renderer->getShader("shaders/post_process.spv");
        //pipeline
        vk::ComputePipelineCreateInfo pipeline_ci
        {
            .stage =
            {
                .stage = vk::ShaderStageFlagBits::eCompute,
                .module = *post_process_module,
                .pName = "main"
            },
            .layout = *pipeline_layout
        };

        pipeline = renderer->gpu->device->createComputePipelineUnique(nullptr, pipeline_ci, nullptr);

        for (auto& buffer : image_buffers)
        {
            buffer.image = renderer->gpu->memory_manager->GetImage(
                renderer->swapchain->extent.width,
                renderer->swapchain->extent.height,
                vk::Format::eR32G32B32A32Sfloat,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage,
                vk::MemoryPropertyFlagBits::eDeviceLocal
            );
        }

        for (auto& buffer : factor_buffers)
        {
            buffer.image = renderer->gpu->memory_manager->GetImage(
                renderer->swapchain->extent.width,
                renderer->swapchain->extent.height,
                vk::Format::eR8Uint,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage,
                vk::MemoryPropertyFlagBits::eDeviceLocal
            );
        }

        vk::ImageViewCreateInfo image_view_info
        {
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eR32G32B32A32Sfloat,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        for (auto& buffer : image_buffers)
        {
            image_view_info.image = buffer.image->image;
            buffer.image_view = renderer->gpu->device->createImageViewUnique(image_view_info, nullptr);
        }

        image_view_info.format = vk::Format::eR8Uint;
        for (auto& buffer : factor_buffers)
        {
            image_view_info.image = buffer.image->image;
            buffer.image_view = renderer->gpu->device->createImageViewUnique(image_view_info, nullptr);
        }

        history_sampler = renderer->gpu->device->createSamplerUnique({
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eNearest,
            .addressModeU = vk::SamplerAddressMode::eClampToEdge,
            .addressModeV = vk::SamplerAddressMode::eClampToEdge,
            .addressModeW = vk::SamplerAddressMode::eClampToEdge,
            .anisotropyEnable = false,
            .maxAnisotropy = 1.f,
            .compareEnable = false,
            .compareOp = vk::CompareOp::eAlways,
            .borderColor = vk::BorderColor::eFloatOpaqueBlack,
            .unnormalizedCoordinates = true
        });

        factor_sampler = renderer->gpu->device->createSamplerUnique({
            .magFilter = vk::Filter::eNearest,
            .minFilter = vk::Filter::eNearest,
            .mipmapMode = vk::SamplerMipmapMode::eNearest,
            .addressModeU = vk::SamplerAddressMode::eClampToBorder,
            .addressModeV = vk::SamplerAddressMode::eClampToBorder,
            .addressModeW = vk::SamplerAddressMode::eClampToBorder,
            .anisotropyEnable = false,
            .maxAnisotropy = 1.f,
            .compareEnable = false,
            .compareOp = vk::CompareOp::eAlways,
            .borderColor = vk::BorderColor::eIntOpaqueWhite,
            .unnormalizedCoordinates = true
        });
    }

    void PostProcessPipeline::InitWork(vk::CommandBuffer buffer)
    {
        std::vector<vk::ImageMemoryBarrier2KHR> barriers;
        for (const auto& buffer : image_buffers)
        {
            barriers.push_back(
                vk::ImageMemoryBarrier2KHR{
                    .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                    .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = buffer.image->image,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                }
            );
        }
        for (const auto& buffer : factor_buffers)
        {
            barriers.push_back(
                vk::ImageMemoryBarrier2KHR{
                    .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                    .srcAccessMask = vk::AccessFlagBits2::eNone,
                    .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                    .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = buffer.image->image,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                }
            );
        }

        buffer.pipelineBarrier2KHR({
            .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
            .pImageMemoryBarriers = barriers.data()
        });

    }

    vk::UniqueCommandBuffer PostProcessPipeline::getCommandBuffer(vk::ImageView input_colour, vk::ImageView input_normals, vk::ImageView input_motionvectors)
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.commandPool = *renderer->graphics_pool;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = 1;

        auto buffer = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        buffer[0]->begin(begin_info);

        buffer[0]->bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);

        vk::DescriptorImageInfo input_colour_info
        {
            .imageView = input_colour,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorImageInfo input_normal_info
        {
            .imageView = input_normals,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorImageInfo input_motionvector_info
        {
            .imageView = input_motionvectors,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorImageInfo input_factor_info
        {
            .sampler = *factor_sampler,
            .imageView = *factor_buffers[buffer_index].image_view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        vk::DescriptorImageInfo input_image_info
        {
            .sampler = *history_sampler,
            .imageView = *image_buffers[buffer_index].image_view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };

        buffer_index = (buffer_index + 1) % image_buffers.size();

        vk::DescriptorImageInfo output_factor_info
        {
            .imageView = *factor_buffers[buffer_index].image_view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorImageInfo output_image_info
        {
            .imageView = *image_buffers[buffer_index].image_view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        std::array descriptorWrites
        {
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &input_colour_info
            },
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &input_normal_info
            },
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &input_motionvector_info
            },
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &input_factor_info
            },
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 4,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &input_image_info
            },
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 5,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &output_factor_info
            },
            vk::WriteDescriptorSet {
                .dstSet = nullptr,
                .dstBinding = 6,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &output_image_info
            }
        };

        std::array before_barriers {
            vk::ImageMemoryBarrier2KHR{
                .srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image_buffers[buffer_index].image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            },
            vk::ImageMemoryBarrier2KHR{
                .srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = factor_buffers[buffer_index].image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            }
        };

        buffer[0]->pipelineBarrier2KHR({
            .imageMemoryBarrierCount = static_cast<uint32_t>(before_barriers.size()),
            .pImageMemoryBarriers = before_barriers.data()
        });

        buffer[0]->pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *pipeline_layout, 0, descriptorWrites);

        buffer[0]->dispatch((renderer->swapchain->extent.width / 16) + 1, (renderer->swapchain->extent.height / 16) + 1, 1);

        std::array after_barriers {
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eNone,
                .dstAccessMask = vk::AccessFlagBits2::eNone,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image_buffers[buffer_index].image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            },
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eNone,
                .dstAccessMask = vk::AccessFlagBits2::eNone,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = factor_buffers[buffer_index].image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            },
        };

        buffer[0]->pipelineBarrier2KHR({
            .imageMemoryBarrierCount = after_barriers.size(),
            .pImageMemoryBarriers = after_barriers.data()
        });

        buffer[0]->end();

        return std::move(buffer[0]);
    }

    vk::ImageView PostProcessPipeline::getOutputImageView()
    {
        return *image_buffers[buffer_index].image_view;
    }
}