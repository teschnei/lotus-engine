#include "renderer_hybrid.h"
#include <glm/glm.hpp>
#include <fstream>

#include "engine/core.h"
#include "engine/game.h"
#include "engine/config.h"

namespace lotus
{
    RendererHybrid::RendererHybrid(Engine* _engine) : Renderer(_engine)
    {
    }

    RendererHybrid::~RendererHybrid()
    {
        gpu->device->waitIdle();
        if (camera_buffers.view_proj_ubo)
            camera_buffers.view_proj_ubo->unmap();
    }

    Task<> RendererHybrid::Init()
    {
        createRenderpasses();
        createDescriptorSetLayout();
        rasterizer = std::make_unique<RasterPipeline>(this);
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

        current_image = gpu->device->acquireNextImageKHR(*swapchain->swapchain, std::numeric_limits<uint64_t>::max(), *image_ready_sem[current_frame], nullptr);
        raytracer->prepareNextFrame();

        co_await InitWork();
    }

    WorkerTask<> RendererHybrid::InitWork()
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

        std::vector barriers = {
            vk::ImageMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2KHR::eNone,
                .srcAccessMask = vk::AccessFlagBits2KHR::eNone,
                .dstStageMask = vk::PipelineStageFlagBits2KHR::eRayTracingShader,
                .dstAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = rtx_gbuffer.colour.image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            }
        };

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

    void RendererHybrid::generateCommandBuffers()
    {
    }

    void RendererHybrid::createRenderpasses()
    {
        {
            vk::AttachmentDescription color_attachment;
            color_attachment.format = swapchain->image_format;
            color_attachment.samples = vk::SampleCountFlagBits::e1;
            color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
            color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
            color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            color_attachment.initialLayout = vk::ImageLayout::eUndefined;
            color_attachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentDescription depth_attachment;
            depth_attachment.format = gpu->getDepthFormat();
            depth_attachment.samples = vk::SampleCountFlagBits::e1;
            depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
            depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
            depth_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            depth_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            depth_attachment.initialLayout = vk::ImageLayout::eUndefined;
            depth_attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            vk::AttachmentReference color_attachment_ref;
            color_attachment_ref.attachment = 0;
            color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference depth_attachment_ref;
            depth_attachment_ref.attachment = 1;
            depth_attachment_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            vk::SubpassDescription subpass = {};
            subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color_attachment_ref;
            subpass.pDepthStencilAttachment = &depth_attachment_ref;

            vk::SubpassDependency dependency;
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

            std::array<vk::AttachmentDescription, 2> attachments = { color_attachment, depth_attachment };
            vk::RenderPassCreateInfo render_pass_info;
            render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            render_pass_info.pAttachments = attachments.data();
            render_pass_info.subpassCount = 1;
            render_pass_info.pSubpasses = &subpass;
            render_pass_info.dependencyCount = 1;
            render_pass_info.pDependencies = &dependency;

            render_pass = gpu->device->createRenderPassUnique(render_pass_info, nullptr);
        }
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
    }

    void RendererHybrid::createDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding position_sample_layout_binding;
        position_sample_layout_binding.binding = 0;
        position_sample_layout_binding.descriptorCount = 1;
        position_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        position_sample_layout_binding.pImmutableSamplers = nullptr;
        position_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding albedo_sample_layout_binding;
        albedo_sample_layout_binding.binding = 1;
        albedo_sample_layout_binding.descriptorCount = 1;
        albedo_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        albedo_sample_layout_binding.pImmutableSamplers = nullptr;
        albedo_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding light_sample_layout_binding;
        light_sample_layout_binding.binding = 2;
        light_sample_layout_binding.descriptorCount = 1;
        light_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        light_sample_layout_binding.pImmutableSamplers = nullptr;
        light_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding material_index_layout_binding;
        material_index_layout_binding.binding = 3;
        material_index_layout_binding.descriptorCount = 1;
        material_index_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        material_index_layout_binding.pImmutableSamplers = nullptr;
        material_index_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

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

        vk::DescriptorSetLayoutBinding mesh_info_binding;
        mesh_info_binding.binding = 6;
        mesh_info_binding.descriptorCount = 1;
        mesh_info_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
        mesh_info_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding light_buffer_binding;
        light_buffer_binding.binding = 7;
        light_buffer_binding.descriptorCount = 1;
        light_buffer_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
        light_buffer_binding.pImmutableSamplers = nullptr;
        light_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding material_info_layout_binding;
        material_info_layout_binding.binding = 8;
        material_info_layout_binding.descriptorCount = GlobalResources::max_resource_index;
        material_info_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        material_info_layout_binding.pImmutableSamplers = nullptr;
        material_info_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding camera_buffer_binding;
        camera_buffer_binding.binding = 9;
        camera_buffer_binding.descriptorCount = 1;
        camera_buffer_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        camera_buffer_binding.pImmutableSamplers = nullptr;
        camera_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        std::vector<vk::DescriptorSetLayoutBinding> rtx_deferred_bindings = { position_sample_layout_binding, albedo_sample_layout_binding, light_sample_layout_binding,
            material_index_layout_binding, accumulation_sample_layout_binding, revealage_sample_layout_binding, mesh_info_binding, light_buffer_binding,
            camera_buffer_binding, material_info_layout_binding };

        vk::DescriptorSetLayoutCreateInfo rtx_deferred_layout_info;
        rtx_deferred_layout_info.bindingCount = static_cast<uint32_t>(rtx_deferred_bindings.size());
        rtx_deferred_layout_info.pBindings = rtx_deferred_bindings.data();
        std::vector<vk::DescriptorBindingFlags> binding_flags_deferred{ {}, {}, {}, {}, {}, {}, {}, {}, vk::DescriptorBindingFlagBits::ePartiallyBound, {} };
        vk::DescriptorSetLayoutBindingFlagsCreateInfo layout_flags_deferred { .bindingCount =  static_cast<uint32_t>(binding_flags_deferred.size()), .pBindingFlags = binding_flags_deferred.data() };
        rtx_deferred_layout_info.pNext = &layout_flags_deferred;

        rtx_descriptor_layout_deferred = gpu->device->createDescriptorSetLayoutUnique(rtx_deferred_layout_info, nullptr);

        std::vector<vk::DescriptorPoolSize> pool_sizes_deferred;
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eStorageBuffer, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eUniformBuffer, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eUniformBuffer, GlobalResources::max_resource_index);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eUniformBuffer, 1);

        vk::DescriptorPoolCreateInfo pool_ci;
        pool_ci.maxSets = 3;
        pool_ci.poolSizeCount = static_cast<uint32_t>(pool_sizes_deferred.size());
        pool_ci.pPoolSizes = pool_sizes_deferred.data();
        pool_ci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        descriptor_pool_deferred = gpu->device->createDescriptorPoolUnique(pool_ci, nullptr);

        std::array<vk::DescriptorSetLayout, 3> layouts = { *rtx_descriptor_layout_deferred, *rtx_descriptor_layout_deferred, *rtx_descriptor_layout_deferred };

        vk::DescriptorSetAllocateInfo set_ci;
        set_ci.descriptorPool = *descriptor_pool_deferred;
        set_ci.descriptorSetCount = 3;
        set_ci.pSetLayouts = layouts.data();
        descriptor_set_deferred = gpu->device->allocateDescriptorSetsUnique(set_ci);
    }

    void RendererHybrid::createRaytracingPipeline()
    {
        std::array descriptors
        {
            vk::DescriptorSetLayoutBinding { //light output
                .binding = 0,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR
            },
            vk::DescriptorSetLayoutBinding { //position input
                .binding = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR,
                .pImmutableSamplers = nullptr
            },
            vk::DescriptorSetLayoutBinding { //normal input
                .binding = 2,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR,
                .pImmutableSamplers = nullptr
            },
            vk::DescriptorSetLayoutBinding { //face normal input
                .binding = 3,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR,
                .pImmutableSamplers = nullptr
            },
            vk::DescriptorSetLayoutBinding { //albedo input
                .binding = 4,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR,
                .pImmutableSamplers = nullptr
            },
            vk::DescriptorSetLayoutBinding { //material index input
                .binding = 5,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR,
                .pImmutableSamplers = nullptr
            },
            vk::DescriptorSetLayoutBinding { //light
                .binding = 6,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR,
                .pImmutableSamplers = nullptr
            },
            vk::DescriptorSetLayoutBinding { //camera
                .binding = 7,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR,
                .pImmutableSamplers = nullptr
            }
        };

        raytracer = std::make_unique<RaytracePipeline>(this, "raygen_hybrid.spv", descriptors);
    }

    void RendererHybrid::createGraphicsPipeline()
    {
        {
            //deferred pipeline
            auto vertex_module = getShader("shaders/quad.spv");
            vk::UniqueShaderModule fragment_module = getShader("shaders/deferred_raytrace_hybrid.spv");

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

    void RendererHybrid::createDepthImage()
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

    void RendererHybrid::createFramebuffers()
    {
        frame_buffers.clear();
        for (auto& swapchain_image_view : swapchain->image_views) {
            std::array<vk::ImageView, 2> attachments = {
                *swapchain_image_view,
                *depth_image_view
            };

            vk::FramebufferCreateInfo framebuffer_info = {};
            framebuffer_info.renderPass = *render_pass;
            framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebuffer_info.pAttachments = attachments.data();
            framebuffer_info.width = swapchain->extent.width;
            framebuffer_info.height = swapchain->extent.height;
            framebuffer_info.layers = 1;

            frame_buffers.push_back(gpu->device->createFramebufferUnique(framebuffer_info, nullptr));
        }
    }

    void RendererHybrid::createSyncs()
    {
        vk::FenceCreateInfo fenceInfo;
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
        for (uint32_t i = 0; i < max_pending_frames; ++i)
        {
            frame_fences.push_back(gpu->device->createFenceUnique(fenceInfo, nullptr));
            image_ready_sem.push_back(gpu->device->createSemaphoreUnique({}, nullptr));
            frame_finish_sem.push_back(gpu->device->createSemaphoreUnique({}, nullptr));
        }
        gbuffer_sem = gpu->device->createSemaphoreUnique({}, nullptr);
        compute_sem = gpu->device->createSemaphoreUnique({}, nullptr);
    }

    void RendererHybrid::createGBufferResources()
    {
        rtx_gbuffer.colour.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = rtx_gbuffer.colour.image->image;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = vk::Format::eR32G32B32A32Sfloat;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;
        rtx_gbuffer.colour.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);

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

    vk::UniqueCommandBuffer RendererHybrid::getDeferredCommandBuffer()
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
        albedo_info.imageView = *rasterizer->getGBuffer().albedo.image_view;
        albedo_info.sampler = *rasterizer->getGBuffer().sampler;

        vk::DescriptorImageInfo light_info;
        light_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        light_info.imageView = post_process->getOutputImageView();
        light_info.sampler = *rasterizer->getGBuffer().sampler;

        vk::DescriptorImageInfo position_info;
        position_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        position_info.imageView = *rasterizer->getGBuffer().position.image_view;
        position_info.sampler = *rasterizer->getGBuffer().sampler;

        vk::DescriptorImageInfo deferred_material_index_info;
        deferred_material_index_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        deferred_material_index_info.imageView = *rasterizer->getGBuffer().material.image_view;
        deferred_material_index_info.sampler = *rasterizer->getGBuffer().sampler;

        vk::DescriptorImageInfo accumulation_info;
        accumulation_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        accumulation_info.imageView = *rasterizer->getGBuffer().accumulation.image_view;
        accumulation_info.sampler = *rasterizer->getGBuffer().sampler;

        vk::DescriptorImageInfo revealage_info;
        revealage_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        revealage_info.imageView = *rasterizer->getGBuffer().revealage.image_view;
        revealage_info.sampler = *rasterizer->getGBuffer().sampler;

        vk::DescriptorBufferInfo mesh_info;
        mesh_info.buffer = resources->mesh_info_buffer->buffer;
        mesh_info.offset = sizeof(GlobalResources::MeshInfo) * GlobalResources::max_resource_index * current_frame;
        mesh_info.range = sizeof(GlobalResources::MeshInfo) * GlobalResources::max_resource_index;

        vk::DescriptorBufferInfo light_buffer_info;
        light_buffer_info.buffer = engine->lights->light_buffer->buffer;
        light_buffer_info.offset = current_frame * engine->lights->GetBufferSize();
        light_buffer_info.range = engine->lights->GetBufferSize();

        vk::DescriptorBufferInfo camera_buffer_info;
        camera_buffer_info.buffer = camera_buffers.view_proj_ubo->buffer;
        camera_buffer_info.offset = current_frame * uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData));
        camera_buffer_info.range = sizeof(Component::CameraComponent::CameraData);

        std::vector<vk::WriteDescriptorSet> descriptorWrites {9};

        descriptorWrites[0].dstSet = *descriptor_set_deferred[current_frame];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &position_info;

        descriptorWrites[1].dstSet = *descriptor_set_deferred[current_frame];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &albedo_info;

        descriptorWrites[2].dstSet = *descriptor_set_deferred[current_frame];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo = &light_info;

        descriptorWrites[3].dstSet = *descriptor_set_deferred[current_frame];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pImageInfo = &deferred_material_index_info;

        descriptorWrites[4].dstSet = *descriptor_set_deferred[current_frame];
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pImageInfo = &accumulation_info;

        descriptorWrites[5].dstSet = *descriptor_set_deferred[current_frame];
        descriptorWrites[5].dstBinding = 5;
        descriptorWrites[5].dstArrayElement = 0;
        descriptorWrites[5].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrites[5].descriptorCount = 1;
        descriptorWrites[5].pImageInfo = &revealage_info;

        descriptorWrites[6].dstSet = *descriptor_set_deferred[current_frame];
        descriptorWrites[6].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[6].dstBinding = 6;
        descriptorWrites[6].dstArrayElement = 0;
        descriptorWrites[6].descriptorCount = 1;
        descriptorWrites[6].pBufferInfo = &mesh_info;

        descriptorWrites[7].dstSet = *descriptor_set_deferred[current_frame];
        descriptorWrites[7].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[7].dstBinding = 7;
        descriptorWrites[7].dstArrayElement = 0;
        descriptorWrites[7].descriptorCount = 1;
        descriptorWrites[7].pBufferInfo = &light_buffer_info;

        descriptorWrites[8].dstSet = *descriptor_set_deferred[current_frame];
        descriptorWrites[8].descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptorWrites[8].dstBinding = 9;
        descriptorWrites[8].dstArrayElement = 0;
        descriptorWrites[8].descriptorCount = 1;
        descriptorWrites[8].pBufferInfo = &camera_buffer_info;

        auto descriptor_material_info = resources->getMaterialInfo();

        if (descriptor_material_info.size() > 0)
        {
            descriptorWrites.push_back({});
            descriptorWrites[9].dstSet = *descriptor_set_deferred[current_frame];
            descriptorWrites[9].dstBinding = 8;
            descriptorWrites[9].dstArrayElement = 0;
            descriptorWrites[9].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[9].descriptorCount = descriptor_material_info.size();
            descriptorWrites[9].pBufferInfo = descriptor_material_info.data();
        }

        gpu->device->updateDescriptorSets(descriptorWrites, {});
        buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *rtx_deferred_pipeline_layout, 0, *descriptor_set_deferred[current_frame], {});

        buffer.draw(3, 1, 0, 0);

        buffer.endRenderPass();

        buffer.end();

        return std::move(deferred_command_buffers[0]);
    }

    void RendererHybrid::initializeCameraBuffers()
    {
        camera_buffers.view_proj_ubo = engine->renderer->gpu->memory_manager->GetBuffer(engine->renderer->uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)) * getFrameCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        camera_buffers.view_proj_mapped = static_cast<Component::CameraComponent::CameraData*>(camera_buffers.view_proj_ubo->map(0, engine->renderer->uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)) * getFrameCount(), {}));
    }

    std::pair<vk::UniqueCommandBuffer, vk::UniqueCommandBuffer> RendererHybrid::getRenderCommandbuffers()
    {
        auto buffer = gpu->device->allocateCommandBuffersUnique({
            .commandPool = *command_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        });

        buffer[0]->begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });

        auto clear_values = rasterizer->getRenderPassClearValues();

        buffer[0]->beginRenderPass({
            .renderPass = rasterizer->getRenderPass(),
            .framebuffer = *rasterizer->getGBuffer().frame_buffer,
            .renderArea = vk::Rect2D {
                .offset = vk::Offset2D{ 0, 0 },
                .extent = swapchain->extent
            },
            .clearValueCount = static_cast<uint32_t>(clear_values.size()),
            .pClearValues = clear_values.data(),
        }, vk::SubpassContents::eSecondaryCommandBuffers);

        auto secondary_buffers = engine->worker_pool->getSecondaryGraphicsBuffers(current_frame);
        if (!secondary_buffers.empty())
            buffer[0]->executeCommands(secondary_buffers);
        buffer[0]->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
        auto particle_buffers = engine->worker_pool->getParticleGraphicsBuffers(current_frame);
        if (!particle_buffers.empty())
            buffer[0]->executeCommands(particle_buffers);
        buffer[0]->endRenderPass();
        buffer[0]->end();

        vk::DescriptorImageInfo target_image_info_colour {
            .imageView = *rtx_gbuffer.colour.image_view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        vk::DescriptorBufferInfo light_buffer_info{
            .buffer = engine->lights->light_buffer->buffer,
            .offset = getCurrentFrame() * engine->lights->GetBufferSize(),
            .range = engine->lights->GetBufferSize()
        };

        vk::DescriptorImageInfo position_info {
            .sampler = *rasterizer->getGBuffer().sampler,
            .imageView = *rasterizer->getGBuffer().position.image_view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        vk::DescriptorImageInfo normal_info {
            .sampler = *rasterizer->getGBuffer().sampler,
            .imageView = *rasterizer->getGBuffer().normal.image_view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        vk::DescriptorImageInfo face_normal_info {
            .sampler = *rasterizer->getGBuffer().sampler,
            .imageView = *rasterizer->getGBuffer().face_normal.image_view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        vk::DescriptorImageInfo albedo_info {
            .sampler = *rasterizer->getGBuffer().sampler,
            .imageView = *rasterizer->getGBuffer().albedo.image_view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        vk::DescriptorImageInfo material_index_info {
            .sampler = *rasterizer->getGBuffer().sampler,
            .imageView = *rasterizer->getGBuffer().material.image_view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        vk::DescriptorBufferInfo camera_buffer_info {
            .buffer = camera_buffers.view_proj_ubo->buffer,
            .offset = current_frame * uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)),
            .range = sizeof(Component::CameraComponent::CameraData),
        };

        std::array descriptors
        {
            vk::WriteDescriptorSet { //colour output
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &target_image_info_colour,
            },
            vk::WriteDescriptorSet { //light output
                .dstBinding = 6,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &light_buffer_info,
            },
            vk::WriteDescriptorSet { //position input
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &position_info,
            },
            vk::WriteDescriptorSet { //normal input
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &normal_info,
            },
            vk::WriteDescriptorSet { //face normal input
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &face_normal_info,
            },
            vk::WriteDescriptorSet { //albedo input
                .dstBinding = 4,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &albedo_info,
            },
            vk::WriteDescriptorSet { //material index input
                .dstBinding = 5,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &material_index_info,
            },
            vk::WriteDescriptorSet { //camera input
                .dstBinding = 7,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &camera_buffer_info,
            }
        };

        std::array after_writes
        {
            vk::ImageMemoryBarrier2KHR
            {
                .srcStageMask = vk::PipelineStageFlagBits2KHR::eRayTracingShader,
                .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                .dstStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead,
                .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = rasterizer->getGBuffer().normal.image->image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            }
        };

        auto raytrace_buffer = raytracer->getCommandBuffer(descriptors, {}, after_writes);

        return { std::move(buffer[0]), std::move(raytrace_buffer) };
    }
    
    Task<> RendererHybrid::drawFrame()
    {
        if (!engine->game || !engine->game->scene)
            co_return;

        resources->BindResources(current_image);
        engine->worker_pool->deleteFinished();
        gpu->device->waitForFences(*frame_fences[current_frame], true, std::numeric_limits<uint64_t>::max());

        try
        {
            engine->worker_pool->clearProcessed(current_frame);
            swapchain->checkOldSwapchain(current_frame);

            co_await raytracer->prepareFrame(engine);

            engine->worker_pool->beginProcessing(current_frame);

            engine->camera->writeToBuffer(camera_buffers.view_proj_mapped[current_frame]);
            engine->lights->UpdateLightBuffer();

            std::vector<vk::Semaphore> waitSemaphores = { *image_ready_sem[current_frame] };
            std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eRayTracingShaderKHR };
            auto buffers = engine->worker_pool->getPrimaryComputeBuffers(current_frame);
            if (!buffers.empty())
            {
                vk::SubmitInfo submitInfo = {};
                submitInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());
                submitInfo.pCommandBuffers = buffers.data();
                //TODO: make this more fine-grained (having all graphics wait for compute is overkill)
                vk::Semaphore compute_signal_sems[] = { *compute_sem };
                submitInfo.signalSemaphoreCount = 1;
                submitInfo.pSignalSemaphores = compute_signal_sems;

                gpu->compute_queue.submit(submitInfo, nullptr);
                waitSemaphores.push_back(*compute_sem);
                waitStages.push_back(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR | vk::PipelineStageFlagBits::eVertexInput);
            }

            vk::SubmitInfo submitInfo = {};
            submitInfo.waitSemaphoreCount = waitSemaphores.size();
            submitInfo.pWaitSemaphores = waitSemaphores.data();
            submitInfo.pWaitDstStageMask = waitStages.data();

            buffers = engine->worker_pool->getPrimaryGraphicsBuffers(current_frame);
            auto [raster_buffer, raytrace_buffer] = getRenderCommandbuffers();
            buffers.push_back(*raster_buffer);
            buffers.push_back(*raytrace_buffer);

            submitInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());
            submitInfo.pCommandBuffers = buffers.data();

            std::vector<vk::Semaphore> gbuffer_semaphores = { *gbuffer_sem };
            submitInfo.pSignalSemaphores = gbuffer_semaphores.data();
            submitInfo.signalSemaphoreCount = gbuffer_semaphores.size();

            gpu->graphics_queue.submit(submitInfo, nullptr);

            engine->worker_pool->gpuResource(std::move(raster_buffer), std::move(raytrace_buffer));

            //post process
            auto post_buffer = post_process->getCommandBuffer(*rtx_gbuffer.colour.image_view, *rasterizer->getGBuffer().normal.image_view, *rasterizer->getGBuffer().motion_vector.image_view);
            std::vector<vk::PipelineStageFlags> post_process_stage_flags = { vk::PipelineStageFlagBits::eComputeShader };
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &*post_buffer;
            submitInfo.pWaitDstStageMask = post_process_stage_flags.data();

            submitInfo.waitSemaphoreCount = gbuffer_semaphores.size();
            submitInfo.pWaitSemaphores = gbuffer_semaphores.data();
            std::vector<vk::Semaphore> post_sems = { *compute_sem };
            submitInfo.signalSemaphoreCount = post_sems.size();
            submitInfo.pSignalSemaphores = post_sems.data();
            gpu->compute_queue.submit(submitInfo, nullptr);

            engine->worker_pool->gpuResource(std::move(post_buffer));

            //deferred render
            submitInfo.waitSemaphoreCount = post_sems.size();
            submitInfo.pWaitSemaphores = post_sems.data();
            auto deferred_buffer = getDeferredCommandBuffer();
            std::vector<vk::CommandBuffer> deferred_commands{ *deferred_buffer };
            submitInfo.commandBufferCount = static_cast<uint32_t>(deferred_commands.size());
            submitInfo.pCommandBuffers = deferred_commands.data();

            std::vector<vk::Semaphore> frame_sem = { *frame_finish_sem[current_frame] };
            submitInfo.signalSemaphoreCount = frame_sem.size();
            submitInfo.pSignalSemaphores = frame_sem.data();
            gpu->graphics_queue.submit(submitInfo, nullptr);

            engine->worker_pool->gpuResource(std::move(deferred_buffer));

            //ui
            auto ui_buffer = ui->Render();
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &ui_buffer;

            submitInfo.waitSemaphoreCount = frame_sem.size();
            submitInfo.pWaitSemaphores = frame_sem.data();
            submitInfo.signalSemaphoreCount = frame_sem.size();
            submitInfo.pSignalSemaphores = frame_sem.data();
            gpu->device->resetFences(*frame_fences[current_frame]);
            gpu->graphics_queue.submit(submitInfo, *frame_fences[current_frame]);

            vk::PresentInfoKHR presentInfo = {};

            presentInfo.waitSemaphoreCount = frame_sem.size();
            presentInfo.pWaitSemaphores = frame_sem.data();

            std::vector<vk::SwapchainKHR> swap_chains = { *swapchain->swapchain };
            presentInfo.swapchainCount = swap_chains.size();
            presentInfo.pSwapchains = swap_chains.data();

            presentInfo.pImageIndices = &current_image;

            co_await engine->worker_pool->mainThread();
            gpu->present_queue.presentKHR(presentInfo);

            current_frame = (current_frame + 1) % max_pending_frames;
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

    Task<> RendererHybrid::recreateRenderer()
    {
        gpu->device->waitIdle();
        engine->worker_pool->Reset();
        swapchain->recreateSwapchain(current_frame);

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

    vk::Pipeline RendererHybrid::createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info)
    {
        info.layout = rasterizer->getPipelineLayout();
        info.renderPass = rasterizer->getRenderPass();
        std::lock_guard lk{ shutdown_mutex };
        return *pipelines.emplace_back(gpu->device->createGraphicsPipelineUnique(nullptr, info, nullptr));
    }
}
