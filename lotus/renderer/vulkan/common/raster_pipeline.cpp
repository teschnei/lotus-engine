module;

#include <array>
#include <cstdint>
#include <memory>
#include <ranges>
#include <vector>

module lotus;

import :renderer.vulkan.pipelines.raster;

import :renderer.memory;
import :renderer.vulkan.renderer;
import vulkan_hpp;

namespace lotus
{
RasterPipeline::RasterPipeline(Renderer* _renderer) : renderer(_renderer)
{
    descriptor_set_layout = initializeDescriptorSetLayout(renderer);
    pipeline_layout = initializePipelineLayout(renderer, *descriptor_set_layout);
    main_attachment_formats = initializeMainRenderPass(renderer);
    transparency_attachment_formats = initializeTransparentRenderPass(renderer);
    gbuffer = initializeGBuffer(renderer);
}

vk::UniqueDescriptorSetLayout RasterPipeline::initializeDescriptorSetLayout(Renderer* renderer)
{
    std::array static_bindings{vk::DescriptorSetLayoutBinding{// camera
                                                              .binding = 0,
                                                              .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                              .descriptorCount = 2,
                                                              .stageFlags = vk::ShaderStageFlagBits::eVertex},
                               vk::DescriptorSetLayoutBinding{// model
                                                              .binding = 1,
                                                              .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                              .descriptorCount = 1,
                                                              .stageFlags = vk::ShaderStageFlagBits::eVertex}};

    return renderer->gpu->device->createDescriptorSetLayoutUnique({.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptor,
                                                                   .bindingCount = static_cast<uint32_t>(static_bindings.size()),
                                                                   .pBindings = static_bindings.data()},
                                                                  nullptr);
}

vk::UniquePipelineLayout RasterPipeline::initializePipelineLayout(Renderer* renderer, vk::DescriptorSetLayout descriptor_set_layout)
{
    std::array descriptor_layouts = {descriptor_set_layout, renderer->global_descriptors->getDescriptorLayout()};

    // material index
    vk::PushConstantRange push_constant_range{.stageFlags = vk::ShaderStageFlagBits::eFragment, .offset = 0, .size = 4};

    return renderer->gpu->device->createPipelineLayoutUnique({.setLayoutCount = static_cast<uint32_t>(descriptor_layouts.size()),
                                                              .pSetLayouts = descriptor_layouts.data(),
                                                              .pushConstantRangeCount = 1,
                                                              .pPushConstantRanges = &push_constant_range},
                                                             nullptr);
}

vk::PipelineRenderingCreateInfo RasterPipeline::getMainRenderPassInfo()
{
    return {.viewMask = 0,
            .colorAttachmentCount = static_cast<uint32_t>(main_attachment_formats.size()),
            .pColorAttachmentFormats = main_attachment_formats.data(),
            .depthAttachmentFormat = renderer->gpu->getDepthFormat()};
}

vk::PipelineRenderingCreateInfo RasterPipeline::getTransparentRenderPassInfo()
{
    return {.viewMask = 0,
            .colorAttachmentCount = static_cast<uint32_t>(transparency_attachment_formats.size()),
            .pColorAttachmentFormats = transparency_attachment_formats.data(),
            .depthAttachmentFormat = renderer->gpu->getDepthFormat()};
}

std::vector<vk::Format> RasterPipeline::initializeMainRenderPass(Renderer* renderer)
{
    return {vk::Format::eR32G32B32A32Sfloat,
            vk::Format::eR32G32B32A32Sfloat,
            vk::Format::eR32G32B32A32Sfloat,
            vk::Format::eR8G8B8A8Unorm,
            vk::Format::eR16Uint,
            vk::Format::eR8Uint,
            vk::Format::eR32G32B32A32Sfloat};
}

std::vector<vk::Format> RasterPipeline::initializeTransparentRenderPass(Renderer* renderer)
{
    return {vk::Format::eR16G16B16A16Sfloat, vk::Format::eR16Sfloat, vk::Format::eR32G32B32A32Sfloat};
}

RasterPipeline::FramebufferAttachment RasterPipeline::initializeFramebufferAttachment(Renderer* renderer, vk::Extent2D extent, vk::Format format,
                                                                                      vk::ImageUsageFlags usage_flags)
{
    vk::ImageAspectFlags aspectMask =
        (usage_flags & vk::ImageUsageFlagBits::eColorAttachment) ? vk::ImageAspectFlagBits::eColor : vk::ImageAspectFlagBits::eDepth;
    std::unique_ptr<Image> image = renderer->gpu->memory_manager->GetImage(extent.width, extent.height, format, vk::ImageTiling::eOptimal, usage_flags,
                                                                           vk::MemoryPropertyFlagBits::eDeviceLocal);
    vk::UniqueImageView image_view = renderer->gpu->device->createImageViewUnique(
        {.image = image->image,
         .viewType = vk::ImageViewType::e2D,
         .format = format,
         .subresourceRange = {.aspectMask = aspectMask, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}});
    return {.image = std::move(image), .image_view = std::move(image_view)};
}

RasterPipeline::GBuffer RasterPipeline::initializeGBuffer(Renderer* renderer)
{
    auto extent = renderer->swapchain->extent;
    GBuffer gbuffer{
        .position = initializeFramebufferAttachment(renderer, extent, vk::Format::eR32G32B32A32Sfloat,
                                                    vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
        .normal =
            initializeFramebufferAttachment(renderer, extent, vk::Format::eR32G32B32A32Sfloat,
                                            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage),
        .face_normal = initializeFramebufferAttachment(renderer, extent, vk::Format::eR32G32B32A32Sfloat,
                                                       vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
        .albedo = initializeFramebufferAttachment(renderer, extent, vk::Format::eR8G8B8A8Unorm,
                                                  vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
        .material = initializeFramebufferAttachment(renderer, extent, vk::Format::eR16Uint,
                                                    vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
        .light_type =
            initializeFramebufferAttachment(renderer, extent, vk::Format::eR8Uint, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
        .motion_vector =
            initializeFramebufferAttachment(renderer, extent, vk::Format::eR32G32B32A32Sfloat,
                                            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst),
        .accumulation = initializeFramebufferAttachment(renderer, extent, vk::Format::eR16G16B16A16Sfloat,
                                                        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
        .revealage = initializeFramebufferAttachment(renderer, extent, vk::Format::eR16Sfloat,
                                                     vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
        .particle = initializeFramebufferAttachment(renderer, extent, vk::Format::eR32G32B32A32Sfloat,
                                                    vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
        .depth = initializeFramebufferAttachment(renderer, extent, renderer->gpu->getDepthFormat(), vk::ImageUsageFlagBits::eDepthStencilAttachment)};

    std::vector<vk::ImageView> attachments{*gbuffer.position.image_view,      *gbuffer.normal.image_view,       *gbuffer.face_normal.image_view,
                                           *gbuffer.albedo.image_view,        *gbuffer.material.image_view,     *gbuffer.light_type.image_view,
                                           *gbuffer.motion_vector.image_view, *gbuffer.accumulation.image_view, *gbuffer.revealage.image_view,
                                           *gbuffer.particle.image_view,      *gbuffer.depth.image_view};

    gbuffer.sampler = renderer->gpu->device->createSamplerUnique({.magFilter = vk::Filter::eNearest,
                                                                  .minFilter = vk::Filter::eNearest,
                                                                  .mipmapMode = vk::SamplerMipmapMode::eNearest,
                                                                  .addressModeU = vk::SamplerAddressMode::eClampToEdge,
                                                                  .addressModeV = vk::SamplerAddressMode::eClampToEdge,
                                                                  .addressModeW = vk::SamplerAddressMode::eClampToEdge,
                                                                  .anisotropyEnable = false,
                                                                  .maxAnisotropy = 1.f,
                                                                  .compareEnable = false,
                                                                  .compareOp = vk::CompareOp::eAlways,
                                                                  .borderColor = vk::BorderColor::eFloatOpaqueBlack,
                                                                  .unnormalizedCoordinates = false},
                                                                 nullptr);

    return gbuffer;
}

void RasterPipeline::beginRendering(vk::CommandBuffer buffer)
{
    std::vector<vk::ImageMemoryBarrier2> pre_render_transitions;

    for (const auto& i : {gbuffer.position.image->image, gbuffer.normal.image->image, gbuffer.face_normal.image->image, gbuffer.albedo.image->image,
                          gbuffer.material.image->image, gbuffer.light_type.image->image, gbuffer.motion_vector.image->image, gbuffer.accumulation.image->image,
                          gbuffer.revealage.image->image, gbuffer.particle.image->image})
    {
        pre_render_transitions.push_back(
            {.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
             .srcAccessMask = {},
             .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
             .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
             .oldLayout = vk::ImageLayout::eUndefined,
             .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
             .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
             .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
             .image = i,
             .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}});
    }

    pre_render_transitions.push_back(
        {.srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
         .srcAccessMask = {},
         .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
         .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
         .oldLayout = vk::ImageLayout::eUndefined,
         .newLayout = vk::ImageLayout::eDepthAttachmentOptimal,
         .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
         .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
         .image = gbuffer.depth.image->image,
         .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}});

    buffer.pipelineBarrier2(
        {.imageMemoryBarrierCount = static_cast<uint32_t>(pre_render_transitions.size()), .pImageMemoryBarriers = pre_render_transitions.data()});
}

void RasterPipeline::endRendering(vk::CommandBuffer buffer)
{
    std::vector<vk::ImageMemoryBarrier2> post_render_transitions;

    for (const auto& i : {gbuffer.position.image->image, gbuffer.normal.image->image, gbuffer.face_normal.image->image, gbuffer.albedo.image->image,
                          gbuffer.material.image->image, gbuffer.light_type.image->image, gbuffer.accumulation.image->image, gbuffer.revealage.image->image,
                          gbuffer.particle.image->image})
    {
        post_render_transitions.push_back(
            {.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
             .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
             .dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR | vk::PipelineStageFlagBits2::eFragmentShader,
             .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
             .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
             .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
             .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
             .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
             .image = i,
             .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}});
    }

    post_render_transitions.push_back(
        {.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
         .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
         .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
         .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
         .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
         .newLayout = vk::ImageLayout::eGeneral,
         .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
         .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
         .image = gbuffer.motion_vector.image->image,
         .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}});

    buffer.pipelineBarrier2(
        {.imageMemoryBarrierCount = static_cast<uint32_t>(post_render_transitions.size()), .pImageMemoryBarriers = post_render_transitions.data()});
}

void RasterPipeline::beginMainCommandBufferRendering(vk::CommandBuffer buffer, vk::RenderingFlags flags)
{
    std::array colour_attachments{vk::RenderingAttachmentInfo{.imageView = *gbuffer.position.image_view,
                                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                                 .loadOp = vk::AttachmentLoadOp::eClear,
                                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                                 .clearValue = {.color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}},
                                  vk::RenderingAttachmentInfo{.imageView = *gbuffer.normal.image_view,
                                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                                 .loadOp = vk::AttachmentLoadOp::eClear,
                                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                                 .clearValue = {.color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}},
                                  vk::RenderingAttachmentInfo{.imageView = *gbuffer.face_normal.image_view,
                                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                                 .loadOp = vk::AttachmentLoadOp::eClear,
                                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                                 .clearValue = {.color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}},
                                  vk::RenderingAttachmentInfo{.imageView = *gbuffer.albedo.image_view,
                                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                                 .loadOp = vk::AttachmentLoadOp::eClear,
                                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                                 .clearValue = {.color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}},
                                  vk::RenderingAttachmentInfo{.imageView = *gbuffer.material.image_view,
                                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                                 .loadOp = vk::AttachmentLoadOp::eClear,
                                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                                 .clearValue = {.color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}},
                                  vk::RenderingAttachmentInfo{.imageView = *gbuffer.light_type.image_view,
                                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                                 .loadOp = vk::AttachmentLoadOp::eClear,
                                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                                 .clearValue = {.color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}},
                                  vk::RenderingAttachmentInfo{.imageView = *gbuffer.motion_vector.image_view,
                                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                                 .loadOp = vk::AttachmentLoadOp::eClear,
                                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                                 .clearValue = {.color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}}};

    vk::RenderingAttachmentInfo depth_info{.imageView = *gbuffer.depth.image_view,
                                              .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                                              .loadOp = vk::AttachmentLoadOp::eClear,
                                              .storeOp = vk::AttachmentStoreOp::eStore,
                                              .clearValue = {.depthStencil = vk::ClearDepthStencilValue{1.0f, 0}}};

    buffer.beginRendering({.flags = flags,
                              .renderArea = {.extent = renderer->swapchain->extent},
                              .layerCount = 1,
                              .viewMask = 0,
                              .colorAttachmentCount = colour_attachments.size(),
                              .pColorAttachments = colour_attachments.data(),
                              .pDepthAttachment = &depth_info});
}

void RasterPipeline::beginTransparencyCommandBufferRendering(vk::CommandBuffer buffer, vk::RenderingFlags flags)
{
    std::array colour_attachments{vk::RenderingAttachmentInfo{.imageView = *gbuffer.accumulation.image_view,
                                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                                 .loadOp = vk::AttachmentLoadOp::eClear,
                                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                                 .clearValue = {.color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}},
                                  vk::RenderingAttachmentInfo{.imageView = *gbuffer.revealage.image_view,
                                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                                 .loadOp = vk::AttachmentLoadOp::eClear,
                                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                                 .clearValue = {.color = std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}}},
                                  vk::RenderingAttachmentInfo{.imageView = *gbuffer.particle.image_view,
                                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                                 .loadOp = vk::AttachmentLoadOp::eClear,
                                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                                 .clearValue = {.color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}}};

    vk::RenderingAttachmentInfo depth_info{.imageView = *gbuffer.depth.image_view,
                                              .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                                              .loadOp = vk::AttachmentLoadOp::eLoad,
                                              .storeOp = vk::AttachmentStoreOp::eDontCare,
                                              .clearValue = {.depthStencil = vk::ClearDepthStencilValue{1.0f, 0}}};

    buffer.beginRendering({.flags = flags,
                              .renderArea = {.extent = renderer->swapchain->extent},
                              .layerCount = 1,
                              .viewMask = 0,
                              .colorAttachmentCount = colour_attachments.size(),
                              .pColorAttachments = colour_attachments.data(),
                              .pDepthAttachment = &depth_info});
}
} // namespace lotus
