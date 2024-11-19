#include "renderer_raytrace.h"
#include <glm/glm.hpp>
#include <fstream>

#include "engine/core.h"
#include "engine/game.h"
#include "engine/config.h"
#include "engine/light_manager.h"
#include "engine/entity/component/camera_component.h"
#include "engine/renderer/acceleration_structure.h"

namespace lotus
{
    RendererRaytrace::RendererRaytrace(Engine* _engine) : Renderer(_engine)
    {
    }

    RendererRaytrace::~RendererRaytrace()
    {
        gpu->device->waitIdle();
        if (camera_buffers.view_proj_ubo)
            camera_buffers.view_proj_ubo->unmap();
    }

    Task<> RendererRaytrace::Init()
    {
        createDescriptorSetLayout();
        createRaytracingPipeline();
        createGraphicsPipeline();
        createDepthImage();
        createSyncs();
        createCommandPool();
        createGBufferResources();
        createAnimationResources();
        createDeferredImage();
        post_process->Init();

        initializeCameraBuffers();

        render_commandbuffers.resize(getFrameCount());

        current_image = gpu->device->acquireNextImageKHR(*swapchain->swapchain, std::numeric_limits<uint64_t>::max(), *image_ready_sem[current_frame], nullptr).value;
        raytracer->prepareNextFrame();

        co_await InitWork();
    }

    WorkerTask<> RendererRaytrace::InitWork()
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *engine->renderer->graphics_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
        auto command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info);

        vk::ImageMemoryBarrier2KHR barrier_albedo;
        barrier_albedo.oldLayout = vk::ImageLayout::eUndefined;
        barrier_albedo.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier_albedo.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier_albedo.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier_albedo.image = gbuffer.albedo.image->image;
        barrier_albedo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier_albedo.subresourceRange.baseMipLevel = 0;
        barrier_albedo.subresourceRange.levelCount = 1;
        barrier_albedo.subresourceRange.baseArrayLayer = 0;
        barrier_albedo.subresourceRange.layerCount = 1;
        barrier_albedo.srcAccessMask = vk::AccessFlagBits2::eNone;
        barrier_albedo.dstAccessMask = vk::AccessFlagBits2::eShaderWrite;
        barrier_albedo.srcStageMask = vk::PipelineStageFlagBits2::eNone;
        barrier_albedo.dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR;

        vk::ImageMemoryBarrier2KHR barrier_particle = barrier_albedo;
        barrier_particle.image = gbuffer.particle.image->image;

        vk::ImageMemoryBarrier2KHR barrier_light = barrier_albedo;
        barrier_light.image = gbuffer.light.image->image;
        barrier_light.newLayout = vk::ImageLayout::eGeneral;

        vk::ImageMemoryBarrier2KHR barrier_normal = barrier_albedo;
        barrier_normal.image = gbuffer.normal.image->image;
        barrier_normal.newLayout = vk::ImageLayout::eGeneral;

        vk::ImageMemoryBarrier2KHR barrier_motion_vector = barrier_albedo;
        barrier_motion_vector.image = gbuffer.motion_vector.image->image;
        barrier_motion_vector.newLayout = vk::ImageLayout::eGeneral;

        std::vector<vk::ImageMemoryBarrier2KHR> barriers;
        barriers.push_back(barrier_albedo);
        barriers.push_back(barrier_light);
        barriers.push_back(barrier_normal);
        barriers.push_back(barrier_particle);
        barriers.push_back(barrier_motion_vector);

        command_buffer->pipelineBarrier2KHR({
            .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
            .pImageMemoryBarriers = barriers.data()
        });

        post_process->InitWork(*command_buffer);

        command_buffer->end();

        engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(command_buffer));

        co_return;
    }

    void RendererRaytrace::createDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding albedo_sample_layout_binding;
        albedo_sample_layout_binding.binding = 0;
        albedo_sample_layout_binding.descriptorCount = 1;
        albedo_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        albedo_sample_layout_binding.pImmutableSamplers = nullptr;
        albedo_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding light_sample_layout_binding;
        light_sample_layout_binding.binding = 1;
        light_sample_layout_binding.descriptorCount = 1;
        light_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        light_sample_layout_binding.pImmutableSamplers = nullptr;
        light_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding particle_sample_layout_binding;
        particle_sample_layout_binding.binding = 2;
        particle_sample_layout_binding.descriptorCount = 1;
        particle_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        particle_sample_layout_binding.pImmutableSamplers = nullptr;
        particle_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding light_buffer_sample_layout_binding;
        light_buffer_sample_layout_binding.binding = 3;
        light_buffer_sample_layout_binding.descriptorCount = 1;
        light_buffer_sample_layout_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
        light_buffer_sample_layout_binding.pImmutableSamplers = nullptr;
        light_buffer_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding camera_buffer_binding;
        camera_buffer_binding.binding = 9;
        camera_buffer_binding.descriptorCount = 2;
        camera_buffer_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        camera_buffer_binding.pImmutableSamplers = nullptr;
        camera_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

        std::vector<vk::DescriptorSetLayoutBinding> deferred_bindings = { albedo_sample_layout_binding,
            light_sample_layout_binding, light_buffer_sample_layout_binding, particle_sample_layout_binding, camera_buffer_binding };

        vk::DescriptorSetLayoutCreateInfo deferred_layout_info;
        deferred_layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
        deferred_layout_info.bindingCount = static_cast<uint32_t>(deferred_bindings.size());
        deferred_layout_info.pBindings = deferred_bindings.data();

        descriptor_layout_deferred = gpu->device->createDescriptorSetLayoutUnique(deferred_layout_info, nullptr);
    }

    void RendererRaytrace::createRaytracingPipeline()
    {
        std::array descriptors {
            vk::DescriptorSetLayoutBinding { //albedo output
                .binding = 0,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR
            },
            vk::DescriptorSetLayoutBinding { //light output
                .binding = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR
            },
            vk::DescriptorSetLayoutBinding { //normal output
                .binding = 2,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR
            },
            vk::DescriptorSetLayoutBinding { //particle output
                .binding = 3,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR
            },
            vk::DescriptorSetLayoutBinding { //motion vector output
                .binding = 4,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR
            },
            vk::DescriptorSetLayoutBinding { //camera ubo input
                .binding = 5,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 2,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR
            },
            vk::DescriptorSetLayoutBinding { //light
                .binding = 6,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR,
                .pImmutableSamplers = nullptr
            },
        };

        raytracer = std::make_unique<RaytracePipeline>(this, "raygen.spv", descriptors);
    }

    void RendererRaytrace::createGraphicsPipeline()
    {
        {
            //deferred pipeline
            auto vertex_module = getShader("shaders/quad.spv");
            auto fragment_module = getShader("shaders/deferred_raytrace.spv");

            vk::PipelineShaderStageCreateInfo vert_shader_stage_info;
            vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
            vert_shader_stage_info.module = *vertex_module;
            vert_shader_stage_info.pName = "main";

            vk::PipelineShaderStageCreateInfo frag_shader_stage_info;
            frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
            frag_shader_stage_info.module = *fragment_module;
            frag_shader_stage_info.pName = "main";

            std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {vert_shader_stage_info, frag_shader_stage_info};

            vk::PipelineVertexInputStateCreateInfo vertex_input_info;

            vertex_input_info.vertexBindingDescriptionCount = 0;
            vertex_input_info.vertexAttributeDescriptionCount = 0;

            vk::PipelineInputAssemblyStateCreateInfo input_assembly = {};
            input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
            input_assembly.primitiveRestartEnable = false;

            vk::PipelineViewportStateCreateInfo viewport_state;
            viewport_state.viewportCount = 1;
            viewport_state.scissorCount = 1;

            vk::PipelineRasterizationStateCreateInfo rasterizer;
            rasterizer.depthClampEnable = false;
            rasterizer.rasterizerDiscardEnable = false;
            rasterizer.polygonMode = vk::PolygonMode::eFill;
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = vk::CullModeFlagBits::eNone;
            rasterizer.frontFace = vk::FrontFace::eClockwise;
            rasterizer.depthBiasEnable = false;

            vk::PipelineMultisampleStateCreateInfo multisampling;
            multisampling.sampleShadingEnable = false;
            multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

            vk::PipelineDepthStencilStateCreateInfo depth_stencil;
            depth_stencil.depthTestEnable = false;
            depth_stencil.depthWriteEnable = false;
            depth_stencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
            depth_stencil.depthBoundsTestEnable = false;
            depth_stencil.stencilTestEnable = false;

            vk::PipelineColorBlendAttachmentState color_blend_attachment;
            color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
            color_blend_attachment.blendEnable = false;

            std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachment_states(1, color_blend_attachment);

            vk::PipelineColorBlendStateCreateInfo color_blending;
            color_blending.logicOpEnable = false;
            color_blending.logicOp = vk::LogicOp::eCopy;
            color_blending.attachmentCount = static_cast<uint32_t>(color_blend_attachment_states.size());
            color_blending.pAttachments = color_blend_attachment_states.data();
            color_blending.blendConstants[0] = 0.0f;
            color_blending.blendConstants[1] = 0.0f;
            color_blending.blendConstants[2] = 0.0f;
            color_blending.blendConstants[3] = 0.0f;

            std::array<vk::DescriptorSetLayout, 1> descriptor_layouts = { *descriptor_layout_deferred };

            vk::PipelineLayoutCreateInfo pipeline_layout_info;
            pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_layouts.size());
            pipeline_layout_info.pSetLayouts = descriptor_layouts.data();

            deferred_pipeline_layout = gpu->device->createPipelineLayoutUnique(pipeline_layout_info, nullptr);

            std::array dynamic_states{ vk::DynamicState::eViewport, vk::DynamicState::eScissor };

            vk::PipelineDynamicStateCreateInfo dynamic_state
            {
                .dynamicStateCount = dynamic_states.size(),
                .pDynamicStates = dynamic_states.data()
            };

            std::array attachment_formats{ vk::Format::eR32G32B32A32Sfloat };

            vk::PipelineRenderingCreateInfoKHR rendering_info {
                .viewMask = 0,
                .colorAttachmentCount = attachment_formats.size(),
                .pColorAttachmentFormats = attachment_formats.data(),
                .depthAttachmentFormat = gpu->getDepthFormat()
            };

            vk::GraphicsPipelineCreateInfo pipeline_info
            {
                .pNext = &rendering_info,
                .stageCount = static_cast<uint32_t>(shaderStages.size()),
                .pStages = shaderStages.data(),
                .pVertexInputState = &vertex_input_info,
                .pInputAssemblyState = &input_assembly,
                .pViewportState = &viewport_state,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = &depth_stencil,
                .pColorBlendState = &color_blending,
                .pDynamicState = &dynamic_state,
                .layout = *deferred_pipeline_layout,
                .subpass = 0,
                .basePipelineHandle = nullptr
            };

            deferred_pipeline = gpu->device->createGraphicsPipelineUnique(nullptr, pipeline_info, nullptr).value;
        }
    }

    void RendererRaytrace::createDepthImage()
    {
        auto format = gpu->getDepthFormat();

        depth_image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = format;
        image_view_info.image = depth_image->image;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        depth_image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
    }

    static constexpr uint64_t timeline_graphics = 1;
    static constexpr uint64_t timeline_frame_ready = 2;

    void RendererRaytrace::createSyncs()
    {
        vk::SemaphoreTypeCreateInfo semaphore_type
        {
            .semaphoreType = vk::SemaphoreType::eTimeline,
            .initialValue = timeline_frame_ready
        };
        for (auto i = 0; i < max_pending_frames; ++i)
        {
            frame_timeline_sem.push_back(gpu->device->createSemaphoreUnique({
                .pNext = &semaphore_type
            }));
            timeline_sem_base.push_back(0);
        }
    }

    void RendererRaytrace::createGBufferResources()
    {
        gbuffer.albedo.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.light.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.normal.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.particle.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.motion_vector.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = gbuffer.albedo.image->image;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = vk::Format::eR32G32B32A32Sfloat;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;
        gbuffer.albedo.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = gbuffer.light.image->image;
        gbuffer.light.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = gbuffer.particle.image->image;
        gbuffer.particle.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = gbuffer.motion_vector.image->image;
        gbuffer.motion_vector.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = gbuffer.normal.image->image;
        gbuffer.normal.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);

        vk::SamplerCreateInfo sampler_info = {};
        sampler_info.magFilter = vk::Filter::eNearest;
        sampler_info.minFilter = vk::Filter::eNearest;
        sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        sampler_info.anisotropyEnable = false;
        sampler_info.maxAnisotropy = 1.f;
        sampler_info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
        sampler_info.unnormalizedCoordinates = false;
        sampler_info.compareEnable = false;
        sampler_info.compareOp = vk::CompareOp::eAlways;
        sampler_info.mipmapMode = vk::SamplerMipmapMode::eNearest;

        gbuffer.sampler = gpu->device->createSamplerUnique(sampler_info, nullptr);
    }

    vk::UniqueCommandBuffer RendererRaytrace::getDeferredCommandBuffer()
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.commandPool = *command_pool;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = 1;

        auto deferred_command_buffers = gpu->device->allocateCommandBuffersUnique(alloc_info);

        vk::CommandBuffer buffer = *deferred_command_buffers[0];

        buffer.begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });

        std::array barriers = {
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = gbuffer.albedo.image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            },
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = gbuffer.particle.image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            },
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
                .srcAccessMask = {},
                .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = deferred_image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            }
        };

        buffer.pipelineBarrier2KHR({
            .imageMemoryBarrierCount = barriers.size(),
            .pImageMemoryBarriers = barriers.data()
        });

        std::array colour_attachments{
            vk::RenderingAttachmentInfoKHR {
                .imageView = *deferred_image_view,
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = { .color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }}
            }
        };

        vk::RenderingAttachmentInfoKHR depth_info {
            .imageView = *depth_image_view,
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .clearValue = { .depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 }}
        };

        buffer.beginRenderingKHR({
            .renderArea = {
                .extent = swapchain->extent
            },
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = colour_attachments.size(),
            .pColorAttachments = colour_attachments.data(),
            .pDepthAttachment = &depth_info
        });

        buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *deferred_pipeline);

        vk::DescriptorImageInfo albedo_info;
        albedo_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        albedo_info.imageView = *gbuffer.albedo.image_view;
        albedo_info.sampler = *gbuffer.sampler;

        vk::DescriptorImageInfo light_info;
        light_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        light_info.imageView = post_process->getOutputImageView();
        light_info.sampler = *gbuffer.sampler;

        vk::DescriptorImageInfo particle_info;
        particle_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        particle_info.imageView = *gbuffer.particle.image_view;
        particle_info.sampler = *gbuffer.sampler;

        vk::DescriptorBufferInfo light_buffer_info_global;
        light_buffer_info_global.buffer = engine->lights->light_buffer->buffer;
        light_buffer_info_global.offset = getCurrentFrame() * engine->lights->GetBufferSize();
        light_buffer_info_global.range = engine->lights->GetBufferSize();

        std::array camera_buffer_info
        {
            vk::DescriptorBufferInfo {
                .buffer = camera_buffers.view_proj_ubo->buffer,
                .offset = current_frame * uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)),
                .range = sizeof(Component::CameraComponent::CameraData)
            },
            vk::DescriptorBufferInfo {
                .buffer = camera_buffers.view_proj_ubo->buffer,
                .offset = previous_frame * uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)),
                .range = sizeof(Component::CameraComponent::CameraData)
            }
        };

        std::vector<vk::WriteDescriptorSet> descriptorWrites {5};

        descriptorWrites[0].dstSet = nullptr;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &albedo_info;

        descriptorWrites[1].dstSet = nullptr;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &light_info;

        descriptorWrites[2].dstSet = nullptr;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo = &particle_info;

        descriptorWrites[3].dstSet = nullptr;
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &light_buffer_info_global;

        descriptorWrites[4].dstSet = nullptr;
        descriptorWrites[4].descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptorWrites[4].dstBinding = 9;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorCount = camera_buffer_info.size();
        descriptorWrites[4].pBufferInfo = camera_buffer_info.data();

        buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *deferred_pipeline_layout, 0, descriptorWrites);

        vk::Viewport viewport {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)engine->renderer->swapchain->extent.width,
            .height = (float)engine->renderer->swapchain->extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vk::Rect2D scissor {
            .offset = vk::Offset2D{0, 0},
            .extent = engine->renderer->swapchain->extent
        };

        buffer.setScissor(0, scissor);
        buffer.setViewport(0, viewport);

        buffer.draw(3, 1, 0, 0);

        buffer.endRenderingKHR();
        buffer.end();

        return std::move(deferred_command_buffers[0]);
    }

    Task<> RendererRaytrace::recreateRenderer()
    {
        //TODO: only recreate swapchain and framebuffers, don't waitIdle
        gpu->device->waitIdle();
        engine->worker_pool->Reset();
        swapchain->recreateSwapchain(current_image);

        createDepthImage();
        //can skip this if scissor/viewport are dynamic
        createGraphicsPipeline();
        createGBufferResources();
        createAnimationResources();
        post_process->Init();
        //recreate command buffers
        co_await recreateStaticCommandBuffers();
        co_await InitWork();
        co_await ui->ReInit();
    }

    void RendererRaytrace::initializeCameraBuffers()
    {
        camera_buffers.view_proj_ubo = engine->renderer->gpu->memory_manager->GetBuffer(engine->renderer->uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)) * engine->renderer->getFrameCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        camera_buffers.view_proj_mapped = static_cast<Component::CameraComponent::CameraData*>(camera_buffers.view_proj_ubo->map(0, engine->renderer->uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)) * engine->renderer->getFrameCount(), {}));
    }

    vk::CommandBuffer RendererRaytrace::getRenderCommandbuffer()
    {
        std::array before_barriers = {
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                .dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = gbuffer.albedo.image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            },
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2::eNone,
                .srcAccessMask = vk::AccessFlagBits2::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                .dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = gbuffer.particle.image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            }
        };

        vk::DescriptorImageInfo target_image_info_albedo {
            .imageView = *gbuffer.albedo.image_view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorImageInfo target_image_info_normal {
            .imageView = *gbuffer.normal.image_view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorImageInfo target_image_info_light{
            .imageView = *gbuffer.light.image_view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorImageInfo target_image_info_particle {
            .imageView = *gbuffer.particle.image_view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorImageInfo target_image_info_motion_vector {
            .imageView = *gbuffer.motion_vector.image_view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        std::array cam_buffer_info
        {
            vk::DescriptorBufferInfo  {
                .buffer = camera_buffers.view_proj_ubo->buffer,
                .offset = uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)) * current_frame,
                .range = sizeof(Component::CameraComponent::CameraData)
            },
            vk::DescriptorBufferInfo  {
                .buffer = camera_buffers.view_proj_ubo->buffer,
                .offset = uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)) * previous_frame,
                .range = sizeof(Component::CameraComponent::CameraData)
            }
        };

        vk::DescriptorBufferInfo light_buffer_info_global {
            .buffer = engine->lights->light_buffer->buffer,
            .offset = getCurrentFrame() * engine->lights->GetBufferSize(),
            .range = engine->lights->GetBufferSize()
        };

        std::array descriptors
        {
            vk::WriteDescriptorSet { //albedo
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &target_image_info_albedo,
            },
            vk::WriteDescriptorSet { //normal output
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &target_image_info_normal,
            },
            vk::WriteDescriptorSet { //light output
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &target_image_info_light,
            },
            vk::WriteDescriptorSet { //particle output
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &target_image_info_particle,
            },
            vk::WriteDescriptorSet { //motion vector output
                .dstBinding = 4,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &target_image_info_motion_vector
            },
            vk::WriteDescriptorSet { //camera input
                .dstBinding = 5,
                .dstArrayElement = 0,
                .descriptorCount = cam_buffer_info.size(),
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = cam_buffer_info.data(),
            },
            vk::WriteDescriptorSet { //light input
                .dstBinding = 6,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &light_buffer_info_global,
            }
        };

        std::array after_barriers = {
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = gbuffer.light.image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            },
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = gbuffer.normal.image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            },
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = gbuffer.motion_vector.image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            }
        };

        render_commandbuffers[current_frame] = raytracer->getCommandBuffer(descriptors, before_barriers, after_barriers);
        return *render_commandbuffers[current_frame];
    }

    Task<> RendererRaytrace::drawFrame()
    {
        if (!engine->game || !engine->game->scene)
            co_return;

        engine->worker_pool->deleteFinished();
        uint64_t frame_ready_value = timeline_sem_base[current_frame] + timeline_frame_ready;
        gpu->device->waitSemaphores({
            .semaphoreCount = 1,
            .pSemaphores = &*frame_timeline_sem[current_frame],
            .pValues = &frame_ready_value
        }, std::numeric_limits<uint64_t>::max());
        timeline_sem_base[current_frame] = timeline_sem_base[current_frame] + timeline_frame_ready;

        engine->worker_pool->clearProcessed(current_frame);
        swapchain->checkOldSwapchain(current_frame);

        co_await raytracer->prepareFrame(engine);

        global_descriptors->updateDescriptorSet();
        engine->worker_pool->beginProcessing(current_frame);

        engine->camera->writeToBuffer(*(Component::CameraComponent::CameraData*)(((uint8_t*)camera_buffers.view_proj_mapped) + uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)) * current_frame));
        engine->lights->UpdateLightBuffer();

        auto buffers_temp = engine->worker_pool->getPrimaryGraphicsBuffers(current_frame);
        std::vector<vk::CommandBufferSubmitInfoKHR> buffers;
        buffers.resize(buffers_temp.size());
        std::ranges::transform(buffers_temp, buffers.begin(), [](auto buffer) { return vk::CommandBufferSubmitInfoKHR{ .commandBuffer = buffer }; });
        buffers.push_back({
            .commandBuffer = getRenderCommandbuffer()
        });
        //post process
        auto post_buffer = post_process->getCommandBuffer(*gbuffer.light.image_view, *gbuffer.normal.image_view, *gbuffer.motion_vector.image_view);
        buffers.push_back({ .commandBuffer = *post_buffer });

        vk::SemaphoreSubmitInfoKHR graphics_sem {
            .semaphore = *frame_timeline_sem[current_frame],
            .value = timeline_sem_base[current_frame] + timeline_graphics,
            .stageMask = vk::PipelineStageFlagBits2::eAllCommands
        };

        //deferred render
        auto deferred_buffer = getDeferredCommandBuffer();
        auto ui_buffers = ui->Render();
        std::vector<vk::CommandBufferSubmitInfoKHR> deferred_buffers{ {.commandBuffer = *deferred_buffer} };
        //deferred_buffers.resize(1 + ui_buffers.size());
        //std::ranges::transform(ui_buffers, deferred_buffers.begin() + 1, [](auto buffer) { return vk::CommandBufferSubmitInfoKHR{ .commandBuffer = buffer }; });
        deferred_buffers.push_back({ .commandBuffer = prepareDeferredImageForPresent()});

        std::array deferred_waits {
            graphics_sem,
            vk::SemaphoreSubmitInfoKHR {
                .semaphore = *image_ready_sem[current_frame],
                .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
            }
        };

        std::array deferred_signals {
            vk::SemaphoreSubmitInfoKHR {
                .semaphore = *frame_timeline_sem[current_frame],
                .value = timeline_sem_base[current_frame] + timeline_frame_ready,
                .stageMask = vk::PipelineStageFlagBits2::eAllCommands
            },
            vk::SemaphoreSubmitInfoKHR {
                .semaphore = *frame_finish_sem[current_frame],
                .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
            }
        };

        gpu->graphics_queue.submit2KHR({
            vk::SubmitInfo2KHR {
                .commandBufferInfoCount = static_cast<uint32_t>(buffers.size()),
                .pCommandBufferInfos = buffers.data(),
                .signalSemaphoreInfoCount = 1,
                .pSignalSemaphoreInfos = &graphics_sem
            },
            vk::SubmitInfo2KHR {
                .waitSemaphoreInfoCount = deferred_waits.size(),
                .pWaitSemaphoreInfos = deferred_waits.data(),
                .commandBufferInfoCount = static_cast<uint32_t>(deferred_buffers.size()),
                .pCommandBufferInfos = deferred_buffers.data(),
                .signalSemaphoreInfoCount = deferred_signals.size(),
                .pSignalSemaphoreInfos = deferred_signals.data()
            }
        });

        engine->worker_pool->gpuResource(std::move(post_buffer), std::move(deferred_buffer));

        std::vector<vk::Semaphore> present_waits{ *frame_finish_sem[current_frame] };
        std::vector<vk::SwapchainKHR> swap_chains = { *swapchain->swapchain };

        co_await engine->worker_pool->mainThread();

        try
        {
            gpu->present_queue.presentKHR({
                .waitSemaphoreCount = static_cast<uint32_t>(present_waits.size()),
                .pWaitSemaphores = present_waits.data(),
                .swapchainCount = static_cast<uint32_t>(swap_chains.size()),
                .pSwapchains = swap_chains.data(),
                .pImageIndices = &current_image
                });
        }
        catch (vk::OutOfDateKHRError&)
        {
        }

        previous_frame = current_frame;
        current_frame = (current_frame + 1) % max_pending_frames;
        previous_image = current_image;
        try
        {
            current_image = gpu->device->acquireNextImageKHR(*swapchain->swapchain, std::numeric_limits<uint64_t>::max(), *image_ready_sem[current_frame], nullptr).value;
        }
        catch (vk::OutOfDateKHRError&)
        {
        }
        raytracer->prepareNextFrame();


        if (resize)
        {
            resize = false;
            co_await resizeRenderer();
        }
    }
}
