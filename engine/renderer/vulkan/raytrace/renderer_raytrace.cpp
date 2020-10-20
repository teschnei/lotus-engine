#include "renderer_raytrace.h"
#include <glm/glm.hpp>
#include <fstream>

#include "engine/core.h"
#include "engine/game.h"
#include "engine/config.h"
#include "engine/entity/camera.h"
#include "engine/entity/renderable_entity.h"
#include "engine/renderer/acceleration_structure.h"

namespace lotus
{
    RendererRaytrace::RendererRaytrace(Engine* _engine) : RendererRaytraceBase(_engine)
    {
    }

    RendererRaytrace::~RendererRaytrace()
    {
        gpu->device->waitIdle();
        if (mesh_info_buffer)
            mesh_info_buffer->unmap();
        if (camera_buffers.view_proj_ubo)
            camera_buffers.view_proj_ubo->unmap();
    }

    Task<> RendererRaytrace::Init()
    {
        createRenderpasses();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createDepthImage();
        createFramebuffers();
        createSyncs();
        createCommandPool();
        createGBufferResources();
        createQuad();
        createRayTracingResources();
        createAnimationResources();

        initializeCameraBuffers();
        generateCommandBuffers();

        render_commandbuffers.resize(getImageCount());

        co_await [this]() -> WorkerTask<>
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

            vk::ImageMemoryBarrier barrier_albedo;
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
            barrier_albedo.srcAccessMask = {};
            barrier_albedo.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

            vk::ImageMemoryBarrier barrier_light = barrier_albedo;
            barrier_light.image = rtx_gbuffer.light.image->image;

            command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eRayTracingShaderKHR, {}, nullptr, nullptr, {barrier_albedo, barrier_light});

            command_buffer->end();

            engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
            engine->worker_pool->frameQueue(std::move(command_buffer));

            co_return;
        }();
    }

    void RendererRaytrace::generateCommandBuffers()
    {
        createDeferredCommandBuffer();
    }

    void RendererRaytrace::createRayTracingResources()
    {
        if (!shader_binding_table)
        {
            constexpr uint32_t shader_raygencount = 1;
            constexpr uint32_t shader_misscount = 2;
            constexpr uint32_t shader_nonhitcount = shader_raygencount + shader_misscount;
            constexpr uint32_t shader_hitcount = shaders_per_group * 6;
            vk::DeviceSize shader_handle_size = gpu->ray_tracing_properties.shaderGroupHandleSize;
            vk::DeviceSize nonhit_shader_stride = shader_handle_size;
            vk::DeviceSize hit_shader_stride = nonhit_shader_stride;
            vk::DeviceSize shader_offset_raygen = 0;
            vk::DeviceSize shader_offset_miss = (((nonhit_shader_stride * shader_raygencount) / engine->renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment) + 1) * engine->renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment;
            vk::DeviceSize shader_offset_hit = shader_offset_miss + (((nonhit_shader_stride * shader_misscount) / engine->renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment) + 1) * engine->renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment;
            vk::DeviceSize sbt_size = (hit_shader_stride * shader_hitcount) + shader_offset_hit;
            shader_binding_table = gpu->memory_manager->GetBuffer(sbt_size, vk::BufferUsageFlagBits::eRayTracingKHR, vk::MemoryPropertyFlagBits::eHostVisible);

            uint8_t* shader_mapped = static_cast<uint8_t*>(shader_binding_table->map(0, sbt_size, {}));

            std::vector<uint8_t> shader_handle_storage((shader_hitcount + shader_nonhitcount) * shader_handle_size);
            gpu->device->getRayTracingShaderGroupHandlesKHR(*rtx_pipeline, 0, shader_nonhitcount + shader_hitcount, shader_handle_storage.size(), shader_handle_storage.data());
            for (uint32_t i = 0; i < shader_raygencount; ++i)
            {
                memcpy(shader_mapped + shader_offset_raygen + (i * nonhit_shader_stride), shader_handle_storage.data() + (i * shader_handle_size), shader_handle_size);
            }
            for (uint32_t i = 0; i < shader_misscount; ++i)
            {
                memcpy(shader_mapped + shader_offset_miss + (i * nonhit_shader_stride), shader_handle_storage.data() + (shader_handle_size * shader_raygencount) + (i * shader_handle_size), shader_handle_size);
            }
            for (uint32_t i = 0; i < shader_hitcount; ++i)
            {
                memcpy(shader_mapped + shader_offset_hit + (i * hit_shader_stride), shader_handle_storage.data() + (shader_handle_size * shader_nonhitcount) + (i * shader_handle_size), shader_handle_size);
            }
            shader_binding_table->unmap();

            raygenSBT = vk::StridedBufferRegionKHR{ shader_binding_table->buffer, shader_offset_raygen, nonhit_shader_stride, nonhit_shader_stride * shader_raygencount };
            missSBT = vk::StridedBufferRegionKHR{ shader_binding_table->buffer, shader_offset_miss, nonhit_shader_stride, nonhit_shader_stride * shader_misscount };
            hitSBT = vk::StridedBufferRegionKHR{ shader_binding_table->buffer, shader_offset_hit, hit_shader_stride, hit_shader_stride * shader_hitcount };

            std::vector<vk::DescriptorPoolSize> pool_sizes_const;
            pool_sizes_const.emplace_back(vk::DescriptorType::eAccelerationStructureKHR, 1);
            pool_sizes_const.emplace_back(vk::DescriptorType::eStorageBuffer, max_acceleration_binding_index);
            pool_sizes_const.emplace_back(vk::DescriptorType::eStorageBuffer, max_acceleration_binding_index);
            pool_sizes_const.emplace_back(vk::DescriptorType::eCombinedImageSampler, max_acceleration_binding_index);
            pool_sizes_const.emplace_back(vk::DescriptorType::eUniformBuffer, 1);

            vk::DescriptorPoolCreateInfo pool_ci;
            pool_ci.maxSets = 3;
            pool_ci.poolSizeCount = static_cast<uint32_t>(pool_sizes_const.size());
            pool_ci.pPoolSizes = pool_sizes_const.data();
            pool_ci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

            rtx_descriptor_pool_const = gpu->device->createDescriptorPoolUnique(pool_ci, nullptr);

            std::array<vk::DescriptorSetLayout, 3> layouts = { *rtx_descriptor_layout_const, *rtx_descriptor_layout_const, *rtx_descriptor_layout_const };

            vk::DescriptorSetAllocateInfo set_ci;
            set_ci.descriptorPool = *rtx_descriptor_pool_const;
            set_ci.descriptorSetCount = 3;
            set_ci.pSetLayouts = layouts.data();
            rtx_descriptor_sets_const = gpu->device->allocateDescriptorSetsUnique(set_ci);
        }
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
        camera_layout_binding.descriptorCount = 1;
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
        camera_deferred_layout_binding.descriptorCount = 1;
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

        vk::DescriptorSetLayoutBinding raytrace_output_binding_albedo;
        raytrace_output_binding_albedo.binding = 0;
        raytrace_output_binding_albedo.descriptorCount = 1;
        raytrace_output_binding_albedo.descriptorType = vk::DescriptorType::eStorageImage;
        raytrace_output_binding_albedo.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

        vk::DescriptorSetLayoutBinding raytrace_output_binding_light;
        raytrace_output_binding_light.binding = 1;
        raytrace_output_binding_light.descriptorCount = 1;
        raytrace_output_binding_light.descriptorType = vk::DescriptorType::eStorageImage;
        raytrace_output_binding_light.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

        vk::DescriptorSetLayoutBinding camera_ubo_binding;
        camera_ubo_binding.binding = 2;
        camera_ubo_binding.descriptorCount = 1;
        camera_ubo_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        camera_ubo_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

        vk::DescriptorSetLayoutBinding light_binding;
        light_binding.binding = 3;
        light_binding.descriptorCount = 1;
        light_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        light_binding.pImmutableSamplers = nullptr;
        light_binding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR;

        vk::DescriptorSetLayoutBinding acceleration_structure_binding;
        acceleration_structure_binding.binding = 0;
        acceleration_structure_binding.descriptorCount = 1;
        acceleration_structure_binding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
        acceleration_structure_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR;

        vk::DescriptorSetLayoutBinding vertex_buffer_binding;
        vertex_buffer_binding.binding = 1;
        vertex_buffer_binding.descriptorCount = max_acceleration_binding_index;
        vertex_buffer_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
        vertex_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR | vk::ShaderStageFlagBits::eIntersectionKHR;

        vk::DescriptorSetLayoutBinding index_buffer_binding;
        index_buffer_binding.binding = 2;
        index_buffer_binding.descriptorCount = max_acceleration_binding_index;
        index_buffer_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
        index_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR | vk::ShaderStageFlagBits::eIntersectionKHR;

        vk::DescriptorSetLayoutBinding texture_bindings;
        texture_bindings.binding = 3;
        texture_bindings.descriptorCount = max_acceleration_binding_index;
        texture_bindings.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        texture_bindings.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

        vk::DescriptorSetLayoutBinding mesh_info_binding;
        mesh_info_binding.binding = 4;
        mesh_info_binding.descriptorCount = 1;
        mesh_info_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        mesh_info_binding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR | vk::ShaderStageFlagBits::eIntersectionKHR;

        std::vector<vk::DescriptorSetLayoutBinding> rtx_bindings_const
        {
            acceleration_structure_binding,
            vertex_buffer_binding,
            index_buffer_binding,
            texture_bindings,
            mesh_info_binding
        };

        std::vector<vk::DescriptorSetLayoutBinding> rtx_bindings_dynamic
        {
            raytrace_output_binding_albedo,
            raytrace_output_binding_light,
            camera_ubo_binding,
            light_binding
        };

        vk::DescriptorSetLayoutCreateInfo rtx_layout_info_const;
        rtx_layout_info_const.bindingCount = static_cast<uint32_t>(rtx_bindings_const.size());
        rtx_layout_info_const.pBindings = rtx_bindings_const.data();

        std::vector<vk::DescriptorBindingFlags> binding_flags{ {}, vk::DescriptorBindingFlagBits::ePartiallyBound, vk::DescriptorBindingFlagBits::ePartiallyBound,
            vk::DescriptorBindingFlagBits::ePartiallyBound, {} };
        vk::DescriptorSetLayoutBindingFlagsCreateInfo layout_flags{ static_cast<uint32_t>(binding_flags.size()), binding_flags.data() };
        rtx_layout_info_const.pNext = &layout_flags;

        vk::DescriptorSetLayoutCreateInfo rtx_layout_info_dynamic;
        rtx_layout_info_dynamic.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
        rtx_layout_info_dynamic.bindingCount = static_cast<uint32_t>(rtx_bindings_dynamic.size());
        rtx_layout_info_dynamic.pBindings = rtx_bindings_dynamic.data();

        rtx_descriptor_layout_const = gpu->device->createDescriptorSetLayoutUnique(rtx_layout_info_const, nullptr);
        rtx_descriptor_layout_dynamic = gpu->device->createDescriptorSetLayoutUnique(rtx_layout_info_dynamic, nullptr);

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

        vk::DescriptorSetLayoutBinding camera_buffer_binding;
        camera_buffer_binding.binding = 8;
        camera_buffer_binding.descriptorCount = 1;
        camera_buffer_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        camera_buffer_binding.pImmutableSamplers = nullptr;
        camera_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

        std::vector<vk::DescriptorSetLayoutBinding> rtx_deferred_bindings = { albedo_sample_layout_binding, light_sample_layout_binding, camera_buffer_binding };

        vk::DescriptorSetLayoutCreateInfo rtx_deferred_layout_info;
        rtx_deferred_layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
        rtx_deferred_layout_info.bindingCount = static_cast<uint32_t>(rtx_deferred_bindings.size());
        rtx_deferred_layout_info.pBindings = rtx_deferred_bindings.data();

        rtx_descriptor_layout_deferred = gpu->device->createDescriptorSetLayoutUnique(rtx_deferred_layout_info, nullptr);
    }

    void RendererRaytrace::createGraphicsPipeline()
    {
        {
            //ray-tracing pipeline
            auto raygen_shader_module = getShader("shaders/raygen.spv");
            auto miss_shader_module = getShader("shaders/miss.spv");
            auto shadow_miss_shader_module = getShader("shaders/shadow_miss.spv");
            auto closest_hit_shader_module = getShader("shaders/closesthit.spv");
            auto color_hit_shader_module = getShader("shaders/color_hit.spv");
            auto landscape_closest_hit_shader_module = getShader("shaders/landscape_closest_hit.spv");
            auto landscape_color_hit_shader_module = getShader("shaders/landscape_color_hit.spv");
            auto particle_closest_hit_shader_module = getShader("shaders/particle_closest_hit.spv");
            auto particle_color_hit_shader_module = getShader("shaders/particle_color_hit.spv");
            auto particle_intersection_shader_module = getShader("shaders/particle_intersection.spv");
            auto particle_shadow_color_hit_shader_module = getShader("shaders/particle_shadow_color_hit.spv");

            vk::PipelineShaderStageCreateInfo raygen_stage_ci;
            raygen_stage_ci.stage = vk::ShaderStageFlagBits::eRaygenKHR;
            raygen_stage_ci.module = *raygen_shader_module;
            raygen_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo miss_stage_ci;
            miss_stage_ci.stage = vk::ShaderStageFlagBits::eMissKHR;
            miss_stage_ci.module = *miss_shader_module;
            miss_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo shadow_miss_stage_ci;
            shadow_miss_stage_ci.stage = vk::ShaderStageFlagBits::eMissKHR;
            shadow_miss_stage_ci.module = *shadow_miss_shader_module;
            shadow_miss_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo closest_stage_ci;
            closest_stage_ci.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
            closest_stage_ci.module = *closest_hit_shader_module;
            closest_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo color_hit_stage_ci;
            color_hit_stage_ci.stage = vk::ShaderStageFlagBits::eAnyHitKHR;
            color_hit_stage_ci.module = *color_hit_shader_module;
            color_hit_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo landscape_closest_stage_ci;
            landscape_closest_stage_ci.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
            landscape_closest_stage_ci.module = *landscape_closest_hit_shader_module;
            landscape_closest_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo landscape_color_hit_stage_ci;
            landscape_color_hit_stage_ci.stage = vk::ShaderStageFlagBits::eAnyHitKHR;
            landscape_color_hit_stage_ci.module = *landscape_color_hit_shader_module;
            landscape_color_hit_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo particle_closest_stage_ci;
            particle_closest_stage_ci.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
            particle_closest_stage_ci.module = *particle_closest_hit_shader_module;
            particle_closest_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo particle_color_hit_stage_ci;
            particle_color_hit_stage_ci.stage = vk::ShaderStageFlagBits::eAnyHitKHR;
            particle_color_hit_stage_ci.module = *particle_color_hit_shader_module;
            particle_color_hit_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo particle_intersection_stage_ci;
            particle_intersection_stage_ci.stage = vk::ShaderStageFlagBits::eIntersectionKHR;
            particle_intersection_stage_ci.module = *particle_intersection_shader_module;
            particle_intersection_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo particle_shadow_color_hit_stage_ci;
            particle_shadow_color_hit_stage_ci.stage = vk::ShaderStageFlagBits::eAnyHitKHR;
            particle_shadow_color_hit_stage_ci.module = *particle_shadow_color_hit_shader_module;
            particle_shadow_color_hit_stage_ci.pName = "main";

            std::vector<vk::PipelineShaderStageCreateInfo> shaders_ci = { raygen_stage_ci, miss_stage_ci, shadow_miss_stage_ci, closest_stage_ci, color_hit_stage_ci,
                landscape_closest_stage_ci, landscape_color_hit_stage_ci, particle_closest_stage_ci, particle_color_hit_stage_ci, particle_intersection_stage_ci, particle_shadow_color_hit_stage_ci };

            std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shader_group_ci = {
                {
                vk::RayTracingShaderGroupTypeKHR::eGeneral,
                0,
                VK_SHADER_UNUSED_KHR,
                VK_SHADER_UNUSED_KHR,
                VK_SHADER_UNUSED_KHR
                },
                {
                vk::RayTracingShaderGroupTypeKHR::eGeneral,
                1,
                VK_SHADER_UNUSED_KHR,
                VK_SHADER_UNUSED_KHR,
                VK_SHADER_UNUSED_KHR
                },
                {
                vk::RayTracingShaderGroupTypeKHR::eGeneral,
                2,
                VK_SHADER_UNUSED_KHR,
                VK_SHADER_UNUSED_KHR,
                VK_SHADER_UNUSED_KHR
                }
            };
            for (int i = 0; i < shaders_per_group; ++i)
            {
                shader_group_ci.emplace_back(
                    vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                    VK_SHADER_UNUSED_KHR,
                    3,
                    4,
                    VK_SHADER_UNUSED_KHR
                );
            }
            for (int i = 0; i < shaders_per_group; ++i)
            {
                shader_group_ci.emplace_back(
                    vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                    VK_SHADER_UNUSED_KHR,
                    VK_SHADER_UNUSED_KHR,
                    4,
                    VK_SHADER_UNUSED_KHR
                );
            }
            for (int i = 0; i < shaders_per_group; ++i)
            {
                shader_group_ci.emplace_back(
                    vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                    VK_SHADER_UNUSED_KHR,
                    5,
                    6,
                    VK_SHADER_UNUSED_KHR
                );
            }
            for (int i = 0; i < shaders_per_group; ++i)
            {
                shader_group_ci.emplace_back(
                    vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                    VK_SHADER_UNUSED_KHR,
                    VK_SHADER_UNUSED_KHR,
                    6,
                    VK_SHADER_UNUSED_KHR
                );
            }

            for (int i = 0; i < shaders_per_group; ++i)
            {
                shader_group_ci.emplace_back(
                    vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup,
                    VK_SHADER_UNUSED_KHR,
                    7,
                    8,
                    9
                );
            }

            for (int i = 0; i < shaders_per_group; ++i)
            {
                shader_group_ci.emplace_back(
                    vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup,
                    VK_SHADER_UNUSED_KHR,
                    VK_SHADER_UNUSED_KHR,
                    10,
                    9
                );
            }

            std::vector<vk::DescriptorSetLayout> rtx_descriptor_layouts = { *rtx_descriptor_layout_const, *rtx_descriptor_layout_dynamic };
            vk::PipelineLayoutCreateInfo rtx_pipeline_layout_ci;
            rtx_pipeline_layout_ci.pSetLayouts = rtx_descriptor_layouts.data();
            rtx_pipeline_layout_ci.setLayoutCount = static_cast<uint32_t>(rtx_descriptor_layouts.size());

            rtx_pipeline_layout = gpu->device->createPipelineLayoutUnique(rtx_pipeline_layout_ci, nullptr);

            vk::RayTracingPipelineCreateInfoKHR rtx_pipeline_ci;
            rtx_pipeline_ci.maxRecursionDepth = 3;
            rtx_pipeline_ci.stageCount = static_cast<uint32_t>(shaders_ci.size());
            rtx_pipeline_ci.pStages = shaders_ci.data();
            rtx_pipeline_ci.groupCount = static_cast<uint32_t>(shader_group_ci.size());
            rtx_pipeline_ci.pGroups = shader_group_ci.data();
            rtx_pipeline_ci.layout = *rtx_pipeline_layout;

            auto result = gpu->device->createRayTracingPipelineKHRUnique(nullptr, rtx_pipeline_ci, nullptr);
            rtx_pipeline = std::move(result.value);
        }
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
            std::array<vk::ImageView, 2> attachments = {
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

    void RendererRaytrace::createSyncs()
    {
        vk::FenceCreateInfo fenceInfo;
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
        for (uint32_t i = 0; i < max_pending_frames; ++i)
        {
            frame_fences.push_back(gpu->device->createFenceUnique(fenceInfo, nullptr));
            image_ready_sem.push_back(gpu->device->createSemaphoreUnique({}, nullptr));
            frame_finish_sem.push_back(gpu->device->createSemaphoreUnique({}, nullptr));
        }
        compute_sem = gpu->device->createSemaphoreUnique({}, nullptr);
        raytrace_sem = gpu->device->createSemaphoreUnique({}, nullptr);
    }

    void RendererRaytrace::createCommandPool()
    {
        vk::CommandPoolCreateInfo pool_info = {};
        pool_info.queueFamilyIndex = gpu->graphics_queue_index;

        command_pool = gpu->device->createCommandPoolUnique(pool_info, nullptr);
    }

    void RendererRaytrace::createGBufferResources()
    {
        rtx_gbuffer.albedo.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);
        rtx_gbuffer.light.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = rtx_gbuffer.albedo.image->image;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = vk::Format::eR8G8B8A8Unorm;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;
        rtx_gbuffer.albedo.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = rtx_gbuffer.light.image->image;
        image_view_info.format = vk::Format::eR32G32B32A32Sfloat;
        rtx_gbuffer.light.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);

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

        if (!mesh_info_buffer_mapped)
        {
            mesh_info_buffer = gpu->memory_manager->GetBuffer(max_acceleration_binding_index * sizeof(MeshInfo) * getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible);
            mesh_info_buffer_mapped = (MeshInfo*)mesh_info_buffer->map(0, max_acceleration_binding_index * sizeof(MeshInfo) * getImageCount(), {});
        }
    }

    void RendererRaytrace::createDeferredCommandBuffer()
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.commandPool = *command_pool;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = getImageCount();

        deferred_command_buffers = gpu->device->allocateCommandBuffersUnique(alloc_info);

        for (int i = 0; i < deferred_command_buffers.size(); ++i)
        {
            vk::CommandBuffer buffer = *deferred_command_buffers[i];

            vk::CommandBufferBeginInfo begin_info = {};
            begin_info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

            buffer.begin(begin_info);

            vk::ImageMemoryBarrier albedo_barrier;
            albedo_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            albedo_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            albedo_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            albedo_barrier.subresourceRange.baseMipLevel = 0;
            albedo_barrier.subresourceRange.levelCount = 1;
            albedo_barrier.subresourceRange.baseArrayLayer = 0;
            albedo_barrier.subresourceRange.layerCount = 1;
            albedo_barrier.image = rtx_gbuffer.albedo.image->image;
            albedo_barrier.oldLayout = vk::ImageLayout::eGeneral;
            albedo_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            albedo_barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
            albedo_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            vk::ImageMemoryBarrier light_barrier = albedo_barrier;
            light_barrier.image = rtx_gbuffer.light.image->image;

            buffer.pipelineBarrier(vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, {albedo_barrier, light_barrier});

            std::array<vk::ClearValue, 2> clear_values;
            clear_values[0].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
            clear_values[1].depthStencil = 1.f;

            vk::RenderPassBeginInfo renderpass_info;
            renderpass_info.renderPass = *rtx_render_pass;
            renderpass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
            renderpass_info.pClearValues = clear_values.data();
            renderpass_info.renderArea.offset = vk::Offset2D{ 0, 0 };
            renderpass_info.renderArea.extent = swapchain->extent;
            renderpass_info.framebuffer = *frame_buffers[i];
            buffer.beginRenderPass(renderpass_info, vk::SubpassContents::eInline);

            buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *rtx_deferred_pipeline);

            vk::DescriptorImageInfo albedo_info;
            albedo_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            albedo_info.imageView = *rtx_gbuffer.albedo.image_view;
            albedo_info.sampler = *rtx_gbuffer.sampler;

            vk::DescriptorImageInfo light_info;
            light_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            light_info.imageView = *rtx_gbuffer.light.image_view;
            light_info.sampler = *rtx_gbuffer.sampler;

            vk::DescriptorBufferInfo camera_buffer_info;
            camera_buffer_info.buffer = camera_buffers.view_proj_ubo->buffer;
            camera_buffer_info.offset = i * uniform_buffer_align_up(sizeof(Camera::CameraData));
            camera_buffer_info.range = sizeof(Camera::CameraData);

            std::vector<vk::WriteDescriptorSet> descriptorWrites {3};

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
            descriptorWrites[2].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[2].dstBinding = 8;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pBufferInfo = &camera_buffer_info;

            buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *rtx_deferred_pipeline_layout, 0, descriptorWrites);

            buffer.draw(3, 1, 0, 0);

            buffer.endRenderPass();
            buffer.end();
        }
    }

    void RendererRaytrace::createQuad()
    {
        struct Vertex
        {
            float pos[3];
            float normal[3];
            float color[3];
            float uv[2];
            float _pad32;
        };

        std::vector<Vertex> vertex_buffer {
            {{1.f, 1.f, 1.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}, 0.f},
            {{-1.f, 1.f, 1.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}, 0.f},
            {{-1.f, -1.f, 1.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}, 0.f},
            {{1.f, -1.f, 1.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}, 0.f}
        };

        quad.vertex_buffer = gpu->memory_manager->GetBuffer(vertex_buffer.size() * sizeof(Vertex), vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        void* buf_mem = quad.vertex_buffer->map(0, vertex_buffer.size() * sizeof(Vertex), {});
        memcpy(buf_mem, vertex_buffer.data(), vertex_buffer.size() * sizeof(Vertex));
        quad.vertex_buffer->unmap();

        std::vector<uint32_t> index_buffer = { 0,1,2,2,3,0 };
        for (uint32_t i = 0; i < 3; ++i)
        {
            uint32_t indices[6] = { 0,1,2, 2,3,0 };
            for (auto index : indices)
            {
                index_buffer.push_back(i * 4 + index);
            }
        }
        quad.index_count = static_cast<uint32_t>(index_buffer.size());

        quad.index_buffer = gpu->memory_manager->GetBuffer(index_buffer.size() * sizeof(uint32_t), vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        buf_mem = quad.index_buffer->map(0, index_buffer.size() * sizeof(uint32_t), {});
        memcpy(buf_mem, index_buffer.data(), index_buffer.size() * sizeof(uint32_t));
        quad.index_buffer->unmap();
    }

    void RendererRaytrace::createAnimationResources()
    {
        //descriptor set layout
        vk::DescriptorSetLayoutBinding vertex_info_buffer;
        vertex_info_buffer.binding = 0;
        vertex_info_buffer.descriptorCount = 1;
        vertex_info_buffer.descriptorType = vk::DescriptorType::eStorageBuffer;
        vertex_info_buffer.pImmutableSamplers = nullptr;
        vertex_info_buffer.stageFlags = vk::ShaderStageFlagBits::eCompute;

        vk::DescriptorSetLayoutBinding skeleton_info_buffer;
        skeleton_info_buffer.binding = 1;
        skeleton_info_buffer.descriptorCount = 1;
        skeleton_info_buffer.descriptorType = vk::DescriptorType::eStorageBuffer;
        skeleton_info_buffer.pImmutableSamplers = nullptr;
        skeleton_info_buffer.stageFlags = vk::ShaderStageFlagBits::eCompute;

        vk::DescriptorSetLayoutBinding vertex_out_buffer;
        vertex_out_buffer.binding = 2;
        vertex_out_buffer.descriptorCount = 1;
        vertex_out_buffer.descriptorType = vk::DescriptorType::eStorageBuffer;
        vertex_out_buffer.pImmutableSamplers = nullptr;
        vertex_out_buffer.stageFlags = vk::ShaderStageFlagBits::eCompute;

        std::vector<vk::DescriptorSetLayoutBinding> descriptor_bindings = { vertex_info_buffer, skeleton_info_buffer, vertex_out_buffer };

        vk::DescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
        layout_info.bindingCount = static_cast<uint32_t>(descriptor_bindings.size());
        layout_info.pBindings = descriptor_bindings.data();

        animation_descriptor_set_layout = gpu->device->createDescriptorSetLayoutUnique(layout_info, nullptr);

        //pipeline layout
        vk::PipelineLayoutCreateInfo pipeline_layout_ci;
        std::vector<vk::DescriptorSetLayout> layouts = { *animation_descriptor_set_layout };
        pipeline_layout_ci.setLayoutCount = static_cast<uint32_t>(layouts.size());
        pipeline_layout_ci.pSetLayouts = layouts.data();

        vk::PushConstantRange push_constants;
        push_constants.size = sizeof(uint32_t);
        push_constants.offset = 0;
        push_constants.stageFlags = vk::ShaderStageFlagBits::eCompute;

        pipeline_layout_ci.pPushConstantRanges = &push_constants;
        pipeline_layout_ci.pushConstantRangeCount = 1;

        animation_pipeline_layout = gpu->device->createPipelineLayoutUnique(pipeline_layout_ci, nullptr);

        //pipeline
        vk::ComputePipelineCreateInfo pipeline_ci;
        pipeline_ci.layout = *animation_pipeline_layout;

        auto animation_module = getShader("shaders/animation_skin.spv");

        vk::PipelineShaderStageCreateInfo animation_shader_stage_info;
        animation_shader_stage_info.stage = vk::ShaderStageFlagBits::eCompute;
        animation_shader_stage_info.module = *animation_module;
        animation_shader_stage_info.pName = "main";

        pipeline_ci.stage = animation_shader_stage_info;

        animation_pipeline = gpu->device->createComputePipelineUnique(nullptr, pipeline_ci, nullptr);
    }

    Task<> RendererRaytrace::resizeRenderer()
    {
        co_await recreateRenderer();
        if (engine->camera)
        {
            engine->camera->setPerspective(glm::radians(70.f), engine->renderer->swapchain->extent.width / (float)engine->renderer->swapchain->extent.height, 0.01f, 1000.f);
        }
    }

    Task<> RendererRaytrace::recreateRenderer()
    {
        gpu->device->waitIdle();
        engine->worker_pool.reset();
        swapchain->recreateSwapchain(current_image);

        createRenderpasses();
        createDepthImage();
        //can skip this if scissor/viewport are dynamic
        createGraphicsPipeline();
        createFramebuffers();
        createGBufferResources();
        createDeferredCommandBuffer();
        createRayTracingResources();
        createAnimationResources();
        //recreate command buffers
        co_await recreateStaticCommandBuffers();
    }

    void RendererRaytrace::initializeCameraBuffers()
    {
        camera_buffers.view_proj_ubo = engine->renderer->gpu->memory_manager->GetBuffer(engine->renderer->uniform_buffer_align_up(sizeof(Camera::CameraData)) * engine->renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        camera_buffers.view_proj_mapped = static_cast<uint8_t*>(camera_buffers.view_proj_ubo->map(0, engine->renderer->uniform_buffer_align_up(sizeof(Camera::CameraData)) * engine->renderer->getImageCount(), {}));
    }

    vk::CommandBuffer RendererRaytrace::getRenderCommandbuffer(uint32_t image_index)
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.commandPool = *command_pool;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = 1;

        auto buffer = gpu->device->allocateCommandBuffersUnique(alloc_info);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        buffer[0]->begin(begin_info);

        //there's no renderpass for raytraced-only pipeline
        buffer[0]->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline);

        buffer[0]->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline_layout, 0, *rtx_descriptor_sets_const[image_index], {});

        vk::ImageMemoryBarrier albedo_barrier;
        albedo_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        albedo_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        albedo_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        albedo_barrier.subresourceRange.baseMipLevel = 0;
        albedo_barrier.subresourceRange.levelCount = 1;
        albedo_barrier.subresourceRange.baseArrayLayer = 0;
        albedo_barrier.subresourceRange.layerCount = 1;
        albedo_barrier.image = rtx_gbuffer.albedo.image->image;
        albedo_barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        albedo_barrier.newLayout = vk::ImageLayout::eGeneral;
        albedo_barrier.srcAccessMask = {};
        albedo_barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

        vk::ImageMemoryBarrier light_barrier = albedo_barrier;
        albedo_barrier.image = rtx_gbuffer.light.image->image;

        buffer[0]->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eRayTracingShaderKHR, {}, {}, {}, { light_barrier, albedo_barrier });

        vk::WriteDescriptorSet write_info_target_albedo;
        write_info_target_albedo.descriptorCount = 1;
        write_info_target_albedo.descriptorType = vk::DescriptorType::eStorageImage;
        write_info_target_albedo.dstBinding = 0;
        write_info_target_albedo.dstArrayElement = 0;
        vk::DescriptorImageInfo target_image_info_albedo;
        target_image_info_albedo.imageView = *rtx_gbuffer.albedo.image_view;
        target_image_info_albedo.imageLayout = vk::ImageLayout::eGeneral;
        write_info_target_albedo.pImageInfo = &target_image_info_albedo;

        vk::WriteDescriptorSet write_info_target_light;
        write_info_target_light.descriptorCount = 1;
        write_info_target_light.descriptorType = vk::DescriptorType::eStorageImage;
        write_info_target_light.dstBinding = 1;
        write_info_target_light.dstArrayElement = 0;
        vk::DescriptorImageInfo target_image_info_light;
        target_image_info_light.imageView = *rtx_gbuffer.light.image_view;
        target_image_info_light.imageLayout = vk::ImageLayout::eGeneral;
        write_info_target_light.pImageInfo = &target_image_info_light;

        vk::DescriptorBufferInfo cam_buffer_info;
        cam_buffer_info.buffer = camera_buffers.view_proj_ubo->buffer;
        cam_buffer_info.offset = uniform_buffer_align_up(sizeof(Camera::CameraData)) * getCurrentImage();
        cam_buffer_info.range = sizeof(Camera::CameraData);

        vk::WriteDescriptorSet write_info_cam;
        write_info_cam.descriptorCount = 1;
        write_info_cam.descriptorType = vk::DescriptorType::eUniformBuffer;
        write_info_cam.dstBinding = 2;
        write_info_cam.dstArrayElement = 0;
        write_info_cam.pBufferInfo = &cam_buffer_info;

        vk::DescriptorBufferInfo light_buffer_info_global;
        light_buffer_info_global.buffer = engine->lights->light_buffer->buffer;
        light_buffer_info_global.offset = getCurrentImage() * uniform_buffer_align_up(sizeof(engine->lights->light));
        light_buffer_info_global.range = sizeof(engine->lights->light);

        vk::WriteDescriptorSet write_info_light;
        write_info_light.descriptorCount = 1;
        write_info_light.descriptorType = vk::DescriptorType::eUniformBuffer;
        write_info_light.dstBinding = 3;
        write_info_light.dstArrayElement = 0;
        write_info_light.pBufferInfo = &light_buffer_info_global;

        buffer[0]->pushDescriptorSetKHR(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline_layout, 1,
            { write_info_target_albedo, write_info_target_light, write_info_cam, write_info_light });

        buffer[0]->traceRaysKHR(raygenSBT, missSBT, hitSBT, {}, swapchain->extent.width, swapchain->extent.height, 1);

        buffer[0]->end();
        render_commandbuffers[image_index] = std::move(buffer[0]);
        return *render_commandbuffers[image_index];
    }

    Task<> RendererRaytrace::drawFrame()
    {
        if (!engine->game || !engine->game->scene)
            co_return;

        engine->worker_pool->deleteFinished();
        gpu->device->waitForFences(*frame_fences[current_frame], true, std::numeric_limits<uint64_t>::max());

        auto prev_image = current_image;
        try
        {
            current_image = gpu->device->acquireNextImageKHR(*swapchain->swapchain, std::numeric_limits<uint64_t>::max(), *image_ready_sem[current_frame], nullptr);

            engine->worker_pool->clearProcessed(current_image);
            swapchain->checkOldSwapchain(current_image);

            if (raytracer->hasQueries())
            {
                raytracer->runQueries(prev_image);
            }
            co_await engine->game->scene->render();
            engine->worker_pool->beginProcessing(current_image);

            engine->camera->updateBuffers(camera_buffers.view_proj_mapped);
            engine->lights->UpdateLightBuffer();

            std::vector<vk::Semaphore> waitSemaphores = { *image_ready_sem[current_frame] };
            std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eRayTracingShaderKHR };
            auto buffers = engine->worker_pool->getPrimaryComputeBuffers(current_image);
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

            buffers = engine->worker_pool->getPrimaryGraphicsBuffers(current_image);
            buffers.push_back(getRenderCommandbuffer(current_image));

            submitInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());
            submitInfo.pCommandBuffers = buffers.data();

            std::vector<vk::Semaphore> raytrace_semaphores = { *raytrace_sem };
            submitInfo.pSignalSemaphores = raytrace_semaphores.data();
            submitInfo.signalSemaphoreCount = raytrace_semaphores.size();

            gpu->graphics_queue.submit(submitInfo, nullptr);

            submitInfo.waitSemaphoreCount = raytrace_semaphores.size();
            submitInfo.pWaitSemaphores = raytrace_semaphores.data();
            std::vector<vk::CommandBuffer> deferred_commands{ *deferred_command_buffers[current_image] };
            deferred_commands.push_back(ui->Render(current_image));
            submitInfo.commandBufferCount = static_cast<uint32_t>(deferred_commands.size());
            submitInfo.pCommandBuffers = deferred_commands.data();

            std::vector<vk::Semaphore> signalSemaphores = { *frame_finish_sem[current_frame] };
            submitInfo.signalSemaphoreCount = signalSemaphores.size();
            submitInfo.pSignalSemaphores = signalSemaphores.data();

            gpu->device->resetFences(*frame_fences[current_frame]);

            gpu->graphics_queue.submit(submitInfo, *frame_fences[current_frame]);

            vk::PresentInfoKHR presentInfo = {};

            presentInfo.waitSemaphoreCount = signalSemaphores.size();
            presentInfo.pWaitSemaphores = signalSemaphores.data();

            std::vector<vk::SwapchainKHR> swap_chains = { *swapchain->swapchain };
            presentInfo.swapchainCount = swap_chains.size();
            presentInfo.pSwapchains = swap_chains.data();

            presentInfo.pImageIndices = &current_image;

            co_await engine->worker_pool->mainThread();
            gpu->present_queue.presentKHR(presentInfo);
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

        current_frame = (current_frame + 1) % max_pending_frames;
    }

    void RendererRaytrace::populateAccelerationStructure(TopLevelAccelerationStructure* tlas, BottomLevelAccelerationStructure* blas, const glm::mat3x4& mat, uint64_t resource_index, uint32_t mask, uint32_t shader_binding_offset)
    {
        vk::AccelerationStructureInstanceKHR instance{};
        memcpy(&instance.transform, &mat, sizeof(mat));
        instance.accelerationStructureReference = blas->handle;
        instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable);
        instance.mask = mask;
        instance.instanceShaderBindingTableRecordOffset = shaders_per_group * shader_binding_offset;
        instance.instanceCustomIndex = resource_index;
        blas->instanceid = tlas->AddInstance(instance);
    }

    void RendererRaytrace::initEntity(EntityInitializer* initializer, Engine* engine)
    {
        initializer->initEntity(this, engine);
    }

    void RendererRaytrace::drawEntity(EntityInitializer* initializer, Engine* engine)
    {
        initializer->drawEntity(this, engine);
    }
}
