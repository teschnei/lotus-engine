#include "raster_pipeline.h"

#include <ranges>

#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    RasterPipeline::RasterPipeline(Renderer* _renderer) : renderer(_renderer)
    {
        descriptor_set_layout = initializeDescriptorSetLayout(renderer);
        pipeline_layout = initializePipelineLayout(renderer, *descriptor_set_layout);
        render_pass = initializeRenderPass(renderer, *pipeline_layout);
        gbuffer = initializeGBuffer(renderer, *render_pass);
    }

    vk::UniqueDescriptorSetLayout RasterPipeline::initializeDescriptorSetLayout(Renderer* renderer)
    {
        std::array static_bindings
        {
            vk::DescriptorSetLayoutBinding { //camera
                .binding = 0,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 2,
                .stageFlags = vk::ShaderStageFlagBits::eVertex
            },
            vk::DescriptorSetLayoutBinding { //texture
                .binding = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            },
            vk::DescriptorSetLayoutBinding { //model
                .binding = 2,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eVertex
            },
            vk::DescriptorSetLayoutBinding { //mesh
                .binding = 3,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            },
            vk::DescriptorSetLayoutBinding { //material
                .binding = 4,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment
            },
        };

        return renderer->gpu->device->createDescriptorSetLayoutUnique({
            .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
            .bindingCount = static_cast<uint32_t>(static_bindings.size()),
            .pBindings = static_bindings.data()
        }, nullptr);
    }

    vk::UniquePipelineLayout RasterPipeline::initializePipelineLayout(Renderer* renderer, vk::DescriptorSetLayout descriptor_set_layout)
    {
        std::array descriptor_layouts = { descriptor_set_layout };

        //material index
        vk::PushConstantRange push_constant_range
        {
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
            .offset = 0,
            .size = 4
        };

        return renderer->gpu->device->createPipelineLayoutUnique({
            .setLayoutCount = static_cast<uint32_t>(descriptor_layouts.size()),
            .pSetLayouts = descriptor_layouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range
        }, nullptr);
    }

    vk::UniqueRenderPass RasterPipeline::initializeRenderPass(Renderer* renderer, vk::PipelineLayout pipeline_layout)
    {
        std::array attachments {
            vk::AttachmentDescription { //position
                .format = vk::Format::eR32G32B32A32Sfloat,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            },
            vk::AttachmentDescription { //normal
                .format = vk::Format::eR32G32B32A32Sfloat,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            },
            vk::AttachmentDescription { //face normal
                .format = vk::Format::eR32G32B32A32Sfloat,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            },
            vk::AttachmentDescription { //albedo
                .format = vk::Format::eR8G8B8A8Unorm,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            },
            vk::AttachmentDescription { //material
                .format = vk::Format::eR16Uint,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            },
            vk::AttachmentDescription { //light
                .format = vk::Format::eR8Uint,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            },
            vk::AttachmentDescription { //motion vector
                .format = vk::Format::eR32G32B32A32Sfloat,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eDontCare,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eGeneral,
            },
            vk::AttachmentDescription { //accumulation
                .format = vk::Format::eR16G16B16A16Sfloat,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            },
            vk::AttachmentDescription { //revealage
                .format = vk::Format::eR16Sfloat,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            },
            vk::AttachmentDescription { //particle
                .format = vk::Format::eR32G32B32A32Sfloat,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            },
            vk::AttachmentDescription { //depth
                .format = renderer->gpu->getDepthFormat(),
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eDontCare,
                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            },
        };

        std::array color_attachment_refs {
            vk::AttachmentReference { //position
                .attachment = 0,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            },
            vk::AttachmentReference { //normal
                .attachment = 1,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            },
            vk::AttachmentReference { //face normal
                .attachment = 2,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            },
            vk::AttachmentReference { //albedo
                .attachment = 3,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            },
            vk::AttachmentReference { //material
                .attachment = 4,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            },
            vk::AttachmentReference { //light type
                .attachment = 5,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            },
            vk::AttachmentReference { //motion vector
                .attachment = 6,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            }
        };

        vk::AttachmentReference depth_attachment_ref {
            .attachment = 10,
            .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
        };

        std::array color_attachment_transparent_refs {
            vk::AttachmentReference { //accumulation
                .attachment = 7,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            },
            vk::AttachmentReference { //revealage
                .attachment = 8,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            },
            vk::AttachmentReference { //particle
                .attachment = 9,
                .layout = vk::ImageLayout::eColorAttachmentOptimal
            }
        };

        std::vector<uint32_t> preserve_attachment_transparent_refs;
        std::ranges::transform(color_attachment_refs, std::back_inserter(preserve_attachment_transparent_refs), &vk::AttachmentReference::attachment);

        std::array subpasses
        {
            vk::SubpassDescription { //main render pass
                .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                .colorAttachmentCount = static_cast<uint32_t>(color_attachment_refs.size()),
                .pColorAttachments = color_attachment_refs.data(),
                .pDepthStencilAttachment = &depth_attachment_ref
            },
            vk::SubpassDescription { //transparency pass
                .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                .colorAttachmentCount = static_cast<uint32_t>(color_attachment_transparent_refs.size()),
                .pColorAttachments = color_attachment_transparent_refs.data(),
                .pDepthStencilAttachment = &depth_attachment_ref,
                .preserveAttachmentCount = static_cast<uint32_t>(preserve_attachment_transparent_refs.size()),
                .pPreserveAttachments = preserve_attachment_transparent_refs.data()
            }
        };

        std::array dependencies
        {
            vk::SubpassDependency {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite
            },
            vk::SubpassDependency {
                .srcSubpass = 0,
                .dstSubpass = 1,
                .srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests,
                .dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests,
                .srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead
            },
            vk::SubpassDependency {
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .dstStageMask = vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dstAccessMask = vk::AccessFlagBits::eShaderRead
            }
        };

        return renderer->gpu->device->createRenderPassUnique({
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .subpassCount = subpasses.size(),
            .pSubpasses = subpasses.data(),
            .dependencyCount = dependencies.size(),
            .pDependencies = dependencies.data()
        }, nullptr);
    }

    RasterPipeline::FramebufferAttachment RasterPipeline::initializeFramebufferAttachment(Renderer* renderer, vk::Extent2D extent, vk::Format format, vk::ImageUsageFlags usage_flags)
    {
        vk::ImageAspectFlags aspectMask = (usage_flags & vk::ImageUsageFlagBits::eColorAttachment) ? vk::ImageAspectFlagBits::eColor : vk::ImageAspectFlagBits::eDepth;
        std::unique_ptr<Image> image = renderer->gpu->memory_manager->GetImage(extent.width, extent.height, format, vk::ImageTiling::eOptimal, usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        vk::UniqueImageView image_view = renderer->gpu->device->createImageViewUnique({
            .image = image->image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        });
        return {
            .image = std::move(image),
            .image_view = std::move(image_view)
        };
    }

    RasterPipeline::GBuffer RasterPipeline::initializeGBuffer(Renderer* renderer, vk::RenderPass render_pass)
    {
        auto extent = renderer->swapchain->extent;
        GBuffer gbuffer{
            .position = initializeFramebufferAttachment(renderer, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
            .normal = initializeFramebufferAttachment(renderer, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage),
            .face_normal = initializeFramebufferAttachment(renderer, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
            .albedo = initializeFramebufferAttachment(renderer, extent, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
            .material = initializeFramebufferAttachment(renderer, extent, vk::Format::eR16Uint, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
            .light_type = initializeFramebufferAttachment(renderer, extent, vk::Format::eR8Uint, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
            .motion_vector = initializeFramebufferAttachment(renderer, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst),
            .accumulation = initializeFramebufferAttachment(renderer, extent, vk::Format::eR16G16B16A16Sfloat, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
            .revealage = initializeFramebufferAttachment(renderer, extent, vk::Format::eR16Sfloat, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
            .particle = initializeFramebufferAttachment(renderer, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
            .depth = initializeFramebufferAttachment(renderer, extent, renderer->gpu->getDepthFormat(), vk::ImageUsageFlagBits::eDepthStencilAttachment)
        };

        std::vector<vk::ImageView> attachments {
            *gbuffer.position.image_view,
            *gbuffer.normal.image_view,
            *gbuffer.face_normal.image_view,
            *gbuffer.albedo.image_view,
            *gbuffer.material.image_view,
            *gbuffer.light_type.image_view,
            *gbuffer.motion_vector.image_view,
            *gbuffer.accumulation.image_view,
            *gbuffer.revealage.image_view,
            *gbuffer.particle.image_view,
            *gbuffer.depth.image_view
        };

        gbuffer.frame_buffer = renderer->gpu->device->createFramebufferUnique({
            .renderPass = render_pass,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .width = extent.width,
            .height = extent.height,
            .layers = 1
        }, nullptr);

        gbuffer.sampler = renderer->gpu->device->createSamplerUnique({
            .magFilter = vk::Filter::eNearest,
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
            .unnormalizedCoordinates = false
        }, nullptr);

        return gbuffer;
    }

    std::vector<vk::ClearValue> RasterPipeline::getRenderPassClearValues()
    {
        return {
            { .color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }},
            { .color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }},
            { .color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }},
            { .color = std::array<float, 4>{ 0.2f, 0.4f, 0.6f, 1.0f }},
            { .color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }},
            { .color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }},
            { .color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }},
            { .color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }},
            { .color = std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f }},
            { .color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }},
            { .depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 }}
        };
    }
}