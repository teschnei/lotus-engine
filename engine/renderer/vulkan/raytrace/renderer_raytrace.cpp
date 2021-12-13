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
        createRenderpasses();
        createDescriptorSetLayout();
        createRaytracingPipeline();
        createGraphicsPipeline();
        createDepthImage();
        createFramebuffers();
        createSyncs();
        createCommandPool();
        createGBufferResources();
        createAnimationResources();
        post_process->Init();

        initializeCameraBuffers();
        generateCommandBuffers();

        render_commandbuffers.resize(getFrameCount());

        current_image = gpu->device->acquireNextImageKHR(*swapchain->swapchain, std::numeric_limits<uint64_t>::max(), *image_ready_sem[current_frame], nullptr);
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
        barrier_albedo.image = rtx_gbuffer.albedo.image->image;
        barrier_albedo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier_albedo.subresourceRange.baseMipLevel = 0;
        barrier_albedo.subresourceRange.levelCount = 1;
        barrier_albedo.subresourceRange.baseArrayLayer = 0;
        barrier_albedo.subresourceRange.layerCount = 1;
        barrier_albedo.srcAccessMask = vk::AccessFlagBits2KHR::eNone;
        barrier_albedo.dstAccessMask = vk::AccessFlagBits2KHR::eShaderWrite;
        barrier_albedo.srcStageMask = vk::PipelineStageFlagBits2KHR::eNone;
        barrier_albedo.dstStageMask = vk::PipelineStageFlagBits2KHR::eRayTracingShader;

        vk::ImageMemoryBarrier2KHR barrier_particle = barrier_albedo;
        barrier_particle.image = rtx_gbuffer.particle.image->image;

        vk::ImageMemoryBarrier2KHR barrier_light = barrier_albedo;
        barrier_light.image = rtx_gbuffer.light.image->image;
        barrier_light.newLayout = vk::ImageLayout::eGeneral;

        vk::ImageMemoryBarrier2KHR barrier_normal = barrier_albedo;
        barrier_normal.image = rtx_gbuffer.normal.image->image;
        barrier_normal.newLayout = vk::ImageLayout::eGeneral;

        vk::ImageMemoryBarrier2KHR barrier_motion_vector = barrier_albedo;
        barrier_motion_vector.image = rtx_gbuffer.motion_vector.image->image;
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

    void RendererRaytrace::generateCommandBuffers()
    {
    }

    void RendererRaytrace::createRenderpasses()
    {
        vk::AttachmentDescription output_attachment;
        output_attachment.format = swapchain->image_format;
        output_attachment.samples = vk::SampleCountFlagBits::e1;
        output_attachment.loadOp = vk::AttachmentLoadOp::eClear;
        output_attachment.storeOp = vk::AttachmentStoreOp::eStore;
        output_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        output_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        output_attachment.initialLayout = vk::ImageLayout::eUndefined;
        output_attachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::AttachmentDescription depth_attachment;
        depth_attachment.format = gpu->getDepthFormat();
        depth_attachment.samples = vk::SampleCountFlagBits::e1;
        depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
        depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
        depth_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        depth_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        depth_attachment.initialLayout = vk::ImageLayout::eUndefined;
        depth_attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        vk::AttachmentReference deferred_output_attachment_ref;
        deferred_output_attachment_ref.attachment = 0;
        deferred_output_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::AttachmentReference deferred_depth_attachment_ref;
        deferred_depth_attachment_ref.attachment = 1;
        deferred_depth_attachment_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        std::vector<vk::AttachmentReference> deferred_color_attachment_refs = { deferred_output_attachment_ref };

        vk::SubpassDescription rtx_subpass;
        rtx_subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        rtx_subpass.colorAttachmentCount = static_cast<uint32_t>(deferred_color_attachment_refs.size());
        rtx_subpass.pColorAttachments = deferred_color_attachment_refs.data();
        rtx_subpass.pDepthStencilAttachment = &deferred_depth_attachment_ref;

        vk::SubpassDependency dependency_initial;
        dependency_initial.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency_initial.dstSubpass = 0;
        dependency_initial.srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
        dependency_initial.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency_initial.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
        dependency_initial.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        dependency_initial.dependencyFlags = vk::DependencyFlagBits::eByRegion;

        std::vector<vk::AttachmentDescription> gbuffer_attachments = { output_attachment, depth_attachment };
        std::vector<vk::SubpassDescription> subpasses = { rtx_subpass };
        std::vector<vk::SubpassDependency> subpass_dependencies = { dependency_initial };

        vk::RenderPassCreateInfo render_pass_info;
        render_pass_info.attachmentCount = static_cast<uint32_t>(gbuffer_attachments.size());
        render_pass_info.pAttachments = gbuffer_attachments.data();
        render_pass_info.subpassCount = static_cast<uint32_t>(subpasses.size());
        render_pass_info.pSubpasses = subpasses.data();
        render_pass_info.dependencyCount = static_cast<uint32_t>(subpass_dependencies.size());
        render_pass_info.pDependencies = subpass_dependencies.data();

        rtx_render_pass = gpu->device->createRenderPassUnique(render_pass_info, nullptr);
    }

    void RendererRaytrace::createDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding camera_layout_binding;
        camera_layout_binding.binding = 0;
        camera_layout_binding.descriptorCount = 2;
        camera_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        camera_layout_binding.pImmutableSamplers = nullptr;
        camera_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

        vk::DescriptorSetLayoutBinding sample_layout_binding;
        sample_layout_binding.binding = 1;
        sample_layout_binding.descriptorCount = 1;
        sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        sample_layout_binding.pImmutableSamplers = nullptr;
        sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding model_layout_binding;
        model_layout_binding.binding = 2;
        model_layout_binding.descriptorCount = 1;
        model_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        model_layout_binding.pImmutableSamplers = nullptr;
        model_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

        vk::DescriptorSetLayoutBinding mesh_info_layout_binding;
        mesh_info_layout_binding.binding = 3;
        mesh_info_layout_binding.descriptorCount = 1;
        mesh_info_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        mesh_info_layout_binding.pImmutableSamplers = nullptr;
        mesh_info_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding mesh_info_index_layout_binding;
        mesh_info_index_layout_binding.binding = 4;
        mesh_info_index_layout_binding.descriptorCount = 1;
        mesh_info_index_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        mesh_info_index_layout_binding.pImmutableSamplers = nullptr;
        mesh_info_index_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        std::vector<vk::DescriptorSetLayoutBinding> static_bindings = { camera_layout_binding, model_layout_binding, sample_layout_binding, mesh_info_layout_binding, mesh_info_index_layout_binding };

        vk::DescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
        layout_info.bindingCount = static_cast<uint32_t>(static_bindings.size());
        layout_info.pBindings = static_bindings.data();

        static_descriptor_set_layout = gpu->device->createDescriptorSetLayoutUnique(layout_info, nullptr);

        vk::DescriptorSetLayoutBinding pos_sample_layout_binding;
        pos_sample_layout_binding.binding = 0;
        pos_sample_layout_binding.descriptorCount = 1;
        pos_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        pos_sample_layout_binding.pImmutableSamplers = nullptr;
        pos_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding normal_sample_layout_binding;
        normal_sample_layout_binding.binding = 1;
        normal_sample_layout_binding.descriptorCount = 1;
        normal_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        normal_sample_layout_binding.pImmutableSamplers = nullptr;
        normal_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding albedo_sample_layout_binding;
        albedo_sample_layout_binding.binding = 2;
        albedo_sample_layout_binding.descriptorCount = 1;
        albedo_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        albedo_sample_layout_binding.pImmutableSamplers = nullptr;
        albedo_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding material_sample_layout_binding;
        material_sample_layout_binding.binding = 3;
        material_sample_layout_binding.descriptorCount = 1;
        material_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        material_sample_layout_binding.pImmutableSamplers = nullptr;
        material_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding accumulation_sample_layout_binding;
        accumulation_sample_layout_binding.binding = 4;
        accumulation_sample_layout_binding.descriptorCount = 1;
        accumulation_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        accumulation_sample_layout_binding.pImmutableSamplers = nullptr;
        accumulation_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding revealage_sample_layout_binding;
        revealage_sample_layout_binding.binding = 5;
        revealage_sample_layout_binding.descriptorCount = 1;
        revealage_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        revealage_sample_layout_binding.pImmutableSamplers = nullptr;
        revealage_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;


        vk::DescriptorSetLayoutBinding camera_deferred_layout_binding;
        camera_deferred_layout_binding.binding = 6;
        camera_deferred_layout_binding.descriptorCount = 2;
        camera_deferred_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        camera_deferred_layout_binding.pImmutableSamplers = nullptr;
        camera_deferred_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding light_deferred_layout_binding;
        light_deferred_layout_binding.binding = 7;
        light_deferred_layout_binding.descriptorCount = 1;
        light_deferred_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        light_deferred_layout_binding.pImmutableSamplers = nullptr;
        light_deferred_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        std::vector<vk::DescriptorSetLayoutBinding> deferred_bindings = {
            pos_sample_layout_binding, 
            normal_sample_layout_binding, 
            albedo_sample_layout_binding,
            material_sample_layout_binding,
            accumulation_sample_layout_binding,
            revealage_sample_layout_binding,
            camera_deferred_layout_binding,
            light_deferred_layout_binding
        };

        layout_info.bindingCount = static_cast<uint32_t>(deferred_bindings.size());
        layout_info.pBindings = deferred_bindings.data();

        deferred_descriptor_set_layout = gpu->device->createDescriptorSetLayoutUnique(layout_info, nullptr);

        //vk::DescriptorSetLayoutBinding albedo_sample_layout_binding;
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

        std::vector<vk::DescriptorSetLayoutBinding> rtx_deferred_bindings = { albedo_sample_layout_binding,
            light_sample_layout_binding, light_buffer_sample_layout_binding, particle_sample_layout_binding, camera_buffer_binding };

        vk::DescriptorSetLayoutCreateInfo rtx_deferred_layout_info;
        rtx_deferred_layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
        rtx_deferred_layout_info.bindingCount = static_cast<uint32_t>(rtx_deferred_bindings.size());
        rtx_deferred_layout_info.pBindings = rtx_deferred_bindings.data();

        rtx_descriptor_layout_deferred = gpu->device->createDescriptorSetLayoutUnique(rtx_deferred_layout_info, nullptr);
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

            vk::Viewport viewport;
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)swapchain->extent.width;
            viewport.height = (float)swapchain->extent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            vk::Rect2D scissor;
            scissor.offset = vk::Offset2D{0, 0};
            scissor.extent = swapchain->extent;

            vk::PipelineViewportStateCreateInfo viewport_state;
            viewport_state.viewportCount = 1;
            viewport_state.pViewports = &viewport;
            viewport_state.scissorCount = 1;
            viewport_state.pScissors = &scissor;

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

            std::array<vk::DescriptorSetLayout, 1> descriptor_layouts = { *rtx_descriptor_layout_deferred };

            vk::PipelineLayoutCreateInfo pipeline_layout_info;
            pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_layouts.size());
            pipeline_layout_info.pSetLayouts = descriptor_layouts.data();

            rtx_deferred_pipeline_layout = gpu->device->createPipelineLayoutUnique(pipeline_layout_info, nullptr);

            vk::GraphicsPipelineCreateInfo pipeline_info;
            pipeline_info.stageCount = static_cast<uint32_t>(shaderStages.size());
            pipeline_info.pStages = shaderStages.data();
            pipeline_info.pVertexInputState = &vertex_input_info;
            pipeline_info.pInputAssemblyState = &input_assembly;
            pipeline_info.pViewportState = &viewport_state;
            pipeline_info.pRasterizationState = &rasterizer;
            pipeline_info.pMultisampleState = &multisampling;
            pipeline_info.pDepthStencilState = &depth_stencil;
            pipeline_info.pColorBlendState = &color_blending;
            pipeline_info.layout = *rtx_deferred_pipeline_layout;
            pipeline_info.renderPass = *rtx_render_pass;
            pipeline_info.subpass = 0;
            pipeline_info.basePipelineHandle = nullptr;

            rtx_deferred_pipeline = gpu->device->createGraphicsPipelineUnique(nullptr, pipeline_info, nullptr);
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

    void RendererRaytrace::createFramebuffers()
    {
        frame_buffers.clear();
        for (auto& swapchain_image_view : swapchain->image_views) {
            std::vector<vk::ImageView> attachments = {
                *swapchain_image_view,
                *depth_image_view
            };

            vk::FramebufferCreateInfo framebuffer_info = {};
            framebuffer_info.renderPass = *rtx_render_pass;
            framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebuffer_info.pAttachments = attachments.data();
            framebuffer_info.width = swapchain->extent.width;
            framebuffer_info.height = swapchain->extent.height;
            framebuffer_info.layers = 1;

            frame_buffers.push_back(gpu->device->createFramebufferUnique(framebuffer_info, nullptr));
        }
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
        rtx_gbuffer.albedo.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);
        rtx_gbuffer.light.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);
        rtx_gbuffer.normal.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);
        rtx_gbuffer.particle.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);
        rtx_gbuffer.motion_vector.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = rtx_gbuffer.albedo.image->image;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = vk::Format::eR32G32B32A32Sfloat;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;
        rtx_gbuffer.albedo.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = rtx_gbuffer.light.image->image;
        rtx_gbuffer.light.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = rtx_gbuffer.particle.image->image;
        rtx_gbuffer.particle.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = rtx_gbuffer.motion_vector.image->image;
        rtx_gbuffer.motion_vector.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = rtx_gbuffer.normal.image->image;
        rtx_gbuffer.normal.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);

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

        rtx_gbuffer.sampler = gpu->device->createSamplerUnique(sampler_info, nullptr);
    }

    vk::UniqueCommandBuffer RendererRaytrace::getDeferredCommandBuffer()
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.commandPool = *command_pool;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = 1;

        auto deferred_command_buffers = gpu->device->allocateCommandBuffersUnique(alloc_info);

        vk::CommandBuffer buffer = *deferred_command_buffers[0];

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        buffer.begin(begin_info);

        std::array barriers = {
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2KHR::eRayTracingShader,
                .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2KHR::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = rtx_gbuffer.albedo.image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            },
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2KHR::eRayTracingShader,
                .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2KHR::eFragmentShader,
                .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = rtx_gbuffer.particle.image->image,
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

        std::array clear_values
        {
            vk::ClearValue{ .color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f } },
            vk::ClearValue{ .depthStencil = 1.f }
        };

        vk::RenderPassBeginInfo renderpass_info;
        renderpass_info.renderPass = *rtx_render_pass;
        renderpass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        renderpass_info.pClearValues = clear_values.data();
        renderpass_info.renderArea.offset = vk::Offset2D{ 0, 0 };
        renderpass_info.renderArea.extent = swapchain->extent;
        renderpass_info.framebuffer = *frame_buffers[current_image];
        buffer.beginRenderPass(renderpass_info, vk::SubpassContents::eInline);

        buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *rtx_deferred_pipeline);

        vk::DescriptorImageInfo albedo_info;
        albedo_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        albedo_info.imageView = *rtx_gbuffer.albedo.image_view;
        albedo_info.sampler = *rtx_gbuffer.sampler;

        vk::DescriptorImageInfo light_info;
        light_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        light_info.imageView = post_process->getOutputImageView();
        light_info.sampler = *rtx_gbuffer.sampler;

        vk::DescriptorImageInfo particle_info;
        particle_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        particle_info.imageView = *rtx_gbuffer.particle.image_view;
        particle_info.sampler = *rtx_gbuffer.sampler;

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

        buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *rtx_deferred_pipeline_layout, 0, descriptorWrites);

        buffer.draw(3, 1, 0, 0);

        buffer.endRenderPass();
        buffer.end();

        return std::move(deferred_command_buffers[0]);
    }

    Task<> RendererRaytrace::recreateRenderer()
    {
        gpu->device->waitIdle();
        engine->worker_pool->Reset();
        swapchain->recreateSwapchain(current_image);

        createRenderpasses();
        createDepthImage();
        //can skip this if scissor/viewport are dynamic
        createGraphicsPipeline();
        createFramebuffers();
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
                .srcStageMask = vk::PipelineStageFlagBits2KHR::eNone,
                .srcAccessMask = vk::AccessFlagBits2KHR::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2KHR::eRayTracingShader,
                .dstAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = rtx_gbuffer.albedo.image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            },
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2KHR::eNone,
                .srcAccessMask = vk::AccessFlagBits2KHR::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2KHR::eRayTracingShader,
                .dstAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = rtx_gbuffer.particle.image->image,
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
            .imageView = *rtx_gbuffer.albedo.image_view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorImageInfo target_image_info_normal {
            .imageView = *rtx_gbuffer.normal.image_view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorImageInfo target_image_info_light{
            .imageView = *rtx_gbuffer.light.image_view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorImageInfo target_image_info_particle {
            .imageView = *rtx_gbuffer.particle.image_view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorImageInfo target_image_info_motion_vector {
            .imageView = *rtx_gbuffer.motion_vector.image_view,
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

        render_commandbuffers[current_frame] = raytracer->getCommandBuffer(descriptors, before_barriers, {});
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
        resources->BindResources(current_frame);

        try
        {
            engine->worker_pool->clearProcessed(current_frame);
            swapchain->checkOldSwapchain(current_frame);

            co_await raytracer->prepareFrame(engine);

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
            auto post_buffer = post_process->getCommandBuffer(*rtx_gbuffer.light.image_view, *rtx_gbuffer.normal.image_view, *rtx_gbuffer.motion_vector.image_view);
            buffers.push_back({
                .commandBuffer = *post_buffer
            });

            vk::SemaphoreSubmitInfoKHR graphics_sem {
                .semaphore = *frame_timeline_sem[current_frame],
                .value = timeline_sem_base[current_frame] + timeline_graphics,
                .stageMask = vk::PipelineStageFlagBits2KHR::eAllCommands
            };

            //deferred render
            auto deferred_buffer = getDeferredCommandBuffer();
            std::vector<vk::CommandBufferSubmitInfoKHR> deferred_commands{ {.commandBuffer = *deferred_buffer}, { .commandBuffer = ui->Render() }};

            std::array deferred_waits {
                graphics_sem,
                vk::SemaphoreSubmitInfoKHR {
                    .semaphore = *image_ready_sem[current_frame],
                    .stageMask = vk::PipelineStageFlagBits2KHR::eColorAttachmentOutput
                }
            };

            std::array deferred_signals {
                vk::SemaphoreSubmitInfoKHR {
                    .semaphore = *frame_timeline_sem[current_frame],
                    .value = timeline_sem_base[current_frame] + timeline_frame_ready,
                    .stageMask = vk::PipelineStageFlagBits2KHR::eAllCommands
                },
                vk::SemaphoreSubmitInfoKHR {
                    .semaphore = *frame_finish_sem[current_frame],
                    .stageMask = vk::PipelineStageFlagBits2KHR::eColorAttachmentOutput
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
                    .commandBufferInfoCount = static_cast<uint32_t>(deferred_commands.size()),
                    .pCommandBufferInfos = deferred_commands.data(),
                    .signalSemaphoreInfoCount = deferred_signals.size(),
                    .pSignalSemaphoreInfos = deferred_signals.data()
                }
            });

            engine->worker_pool->gpuResource(std::move(post_buffer), std::move(deferred_buffer));

            std::vector<vk::Semaphore> present_waits{ *frame_finish_sem[current_frame] };
            std::vector<vk::SwapchainKHR> swap_chains = { *swapchain->swapchain };

            co_await engine->worker_pool->mainThread();

            gpu->present_queue.presentKHR({
                .waitSemaphoreCount = static_cast<uint32_t>(present_waits.size()),
                .pWaitSemaphores = present_waits.data(),
                .swapchainCount = static_cast<uint32_t>(swap_chains.size()),
                .pSwapchains = swap_chains.data(),
                .pImageIndices = &current_image
            });

            previous_frame = current_frame;
            current_frame = (current_frame + 1) % max_pending_frames;
            previous_image = current_image;
            current_image = gpu->device->acquireNextImageKHR(*swapchain->swapchain, std::numeric_limits<uint64_t>::max(), *image_ready_sem[current_frame], nullptr);
            raytracer->prepareNextFrame();
        }
        catch (vk::OutOfDateKHRError&)
        {
            resize = true;
        }

        if (resize)
        {
            resize = false;
            co_await resizeRenderer();
        }
    }
}
