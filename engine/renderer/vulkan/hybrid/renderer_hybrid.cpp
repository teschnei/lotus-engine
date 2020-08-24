#include "renderer_hybrid.h"
#include <glm/glm.hpp>
#include <fstream>

#include "engine/core.h"
#include "engine/game.h"
#include "engine/config.h"
#include "engine/entity/camera.h"
#include "engine/entity/renderable_entity.h"

namespace lotus
{
    RendererHybrid::RendererHybrid(Engine* _engine) : RendererRaytraceBase(_engine)
    {
    }

    RendererHybrid::~RendererHybrid()
    {
        gpu->device->waitIdle();
        if (mesh_info_buffer)
            mesh_info_buffer->unmap();
        if (camera_buffers.view_proj_ubo)
            camera_buffers.view_proj_ubo->unmap();
    }

    void RendererHybrid::Init()
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
        raytracer = std::make_unique<Raytracer>(engine);
    }

    void RendererHybrid::generateCommandBuffers()
    {
        createDeferredCommandBuffer();
    }

    void RendererHybrid::createRayTracingResources()
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
            vk::DeviceSize shader_offset_miss = (((nonhit_shader_stride * shader_raygencount) / gpu->ray_tracing_properties.shaderGroupBaseAlignment) + 1) * gpu->ray_tracing_properties.shaderGroupBaseAlignment;
            vk::DeviceSize shader_offset_hit = shader_offset_miss + (((nonhit_shader_stride * shader_misscount) / gpu->ray_tracing_properties.shaderGroupBaseAlignment) + 1) * gpu->ray_tracing_properties.shaderGroupBaseAlignment;
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
            rtx_descriptor_sets_const = gpu->device->allocateDescriptorSetsUnique<std::allocator<vk::UniqueHandle<vk::DescriptorSet, vk::DispatchLoaderDynamic>>>(set_ci);
        }
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
            color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

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

            vk::AttachmentDescription desc_pos;
            desc_pos.format = vk::Format::eR32G32B32A32Sfloat;
            desc_pos.samples = vk::SampleCountFlagBits::e1;
            desc_pos.loadOp = vk::AttachmentLoadOp::eClear;
            desc_pos.storeOp = vk::AttachmentStoreOp::eStore;
            desc_pos.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_pos.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_pos.initialLayout = vk::ImageLayout::eUndefined;
            desc_pos.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentDescription desc_normal;
            desc_normal.format = vk::Format::eR32G32B32A32Sfloat;
            desc_normal.samples = vk::SampleCountFlagBits::e1;
            desc_normal.loadOp = vk::AttachmentLoadOp::eClear;
            desc_normal.storeOp = vk::AttachmentStoreOp::eStore;
            desc_normal.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_normal.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_normal.initialLayout = vk::ImageLayout::eUndefined;
            desc_normal.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentDescription desc_albedo;
            desc_albedo.format = vk::Format::eR8G8B8A8Unorm;
            desc_albedo.samples = vk::SampleCountFlagBits::e1;
            desc_albedo.loadOp = vk::AttachmentLoadOp::eClear;
            desc_albedo.storeOp = vk::AttachmentStoreOp::eStore;
            desc_albedo.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_albedo.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_albedo.initialLayout = vk::ImageLayout::eUndefined;
            desc_albedo.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentDescription desc_accumulation;
            desc_accumulation.format = vk::Format::eR16G16B16A16Sfloat;
            desc_accumulation.samples = vk::SampleCountFlagBits::e1;
            desc_accumulation.loadOp = vk::AttachmentLoadOp::eClear;
            desc_accumulation.storeOp = vk::AttachmentStoreOp::eStore;
            desc_accumulation.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_accumulation.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_accumulation.initialLayout = vk::ImageLayout::eUndefined;
            desc_accumulation.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentDescription desc_revealage;
            desc_revealage.format = vk::Format::eR16Sfloat;
            desc_revealage.samples = vk::SampleCountFlagBits::e1;
            desc_revealage.loadOp = vk::AttachmentLoadOp::eClear;
            desc_revealage.storeOp = vk::AttachmentStoreOp::eStore;
            desc_revealage.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_revealage.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_revealage.initialLayout = vk::ImageLayout::eUndefined;
            desc_revealage.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentDescription desc_face_normal;
            desc_face_normal.format = vk::Format::eR32G32B32A32Sfloat;
            desc_face_normal.samples = vk::SampleCountFlagBits::e1;
            desc_face_normal.loadOp = vk::AttachmentLoadOp::eClear;
            desc_face_normal.storeOp = vk::AttachmentStoreOp::eStore;
            desc_face_normal.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_face_normal.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_face_normal.initialLayout = vk::ImageLayout::eUndefined;
            desc_face_normal.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentDescription desc_material;
            desc_material.format = vk::Format::eR16Uint;
            desc_material.samples = vk::SampleCountFlagBits::e1;
            desc_material.loadOp = vk::AttachmentLoadOp::eClear;
            desc_material.storeOp = vk::AttachmentStoreOp::eStore;
            desc_material.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_material.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_material.initialLayout = vk::ImageLayout::eUndefined;
            desc_material.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            std::vector<vk::AttachmentDescription> gbuffer_attachments = { desc_pos, desc_normal, desc_face_normal, desc_albedo, desc_accumulation, desc_revealage, desc_material, depth_attachment };

            render_pass_info.attachmentCount = static_cast<uint32_t>(gbuffer_attachments.size());
            render_pass_info.pAttachments = gbuffer_attachments.data();

            vk::AttachmentReference gbuffer_pos_attachment_ref;
            gbuffer_pos_attachment_ref.attachment = 0;
            gbuffer_pos_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_normal_attachment_ref;
            gbuffer_normal_attachment_ref.attachment = 1;
            gbuffer_normal_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_face_normal_attachment_ref;
            gbuffer_face_normal_attachment_ref.attachment = 2;
            gbuffer_face_normal_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_albedo_attachment_ref;
            gbuffer_albedo_attachment_ref.attachment = 3;
            gbuffer_albedo_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_accumulation_attachment_ref;
            gbuffer_accumulation_attachment_ref.attachment = 4;
            gbuffer_accumulation_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_revealage_attachment_ref;
            gbuffer_revealage_attachment_ref.attachment = 5;
            gbuffer_revealage_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_material_attachment_ref;
            gbuffer_material_attachment_ref.attachment = 6;
            gbuffer_material_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_depth_attachment_ref;
            gbuffer_depth_attachment_ref.attachment = 7;
            gbuffer_depth_attachment_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            std::vector<vk::AttachmentReference> gbuffer_color_attachment_refs = { gbuffer_pos_attachment_ref, gbuffer_normal_attachment_ref, gbuffer_face_normal_attachment_ref,
                gbuffer_albedo_attachment_ref, gbuffer_material_attachment_ref };

            vk::SubpassDescription subpass_deferred;
            subpass_deferred.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            subpass_deferred.colorAttachmentCount = static_cast<uint32_t>(gbuffer_color_attachment_refs.size());
            subpass_deferred.pColorAttachments = gbuffer_color_attachment_refs.data();
            subpass_deferred.pDepthStencilAttachment = &gbuffer_depth_attachment_ref;

            std::vector<vk::AttachmentReference> gbuffer_color_attachment_transparent_refs = { gbuffer_accumulation_attachment_ref, gbuffer_revealage_attachment_ref };
            std::vector<uint32_t> gbuffer_preserve_attachment_transparent_refs = { gbuffer_pos_attachment_ref.attachment, gbuffer_normal_attachment_ref.attachment,
                gbuffer_face_normal_attachment_ref.attachment, gbuffer_albedo_attachment_ref.attachment, gbuffer_material_attachment_ref.attachment };
            vk::SubpassDescription subpass_transparent;
            subpass_transparent.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            subpass_transparent.colorAttachmentCount = static_cast<uint32_t>(gbuffer_color_attachment_transparent_refs.size());
            subpass_transparent.pColorAttachments = gbuffer_color_attachment_transparent_refs.data();
            subpass_transparent.preserveAttachmentCount = gbuffer_preserve_attachment_transparent_refs.size();
            subpass_transparent.pPreserveAttachments = gbuffer_preserve_attachment_transparent_refs.data();
            subpass_transparent.pDepthStencilAttachment = &gbuffer_depth_attachment_ref;

            vk::SubpassDependency gbuffer_dependency_end;
            gbuffer_dependency_end.srcSubpass = 0;
            gbuffer_dependency_end.dstSubpass = VK_SUBPASS_EXTERNAL;
            gbuffer_dependency_end.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            gbuffer_dependency_end.dstStageMask = vk::PipelineStageFlagBits::eRayTracingShaderKHR;
            gbuffer_dependency_end.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            gbuffer_dependency_end.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            vk::SubpassDependency gbuffer_dependency_transparent;
            gbuffer_dependency_transparent.srcSubpass = 0;
            gbuffer_dependency_transparent.dstSubpass = 1;
            gbuffer_dependency_transparent.srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests;
            gbuffer_dependency_transparent.dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;
            gbuffer_dependency_transparent.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            gbuffer_dependency_transparent.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead;

            std::vector<vk::SubpassDependency> dependencies = { dependency, gbuffer_dependency_end, gbuffer_dependency_transparent };
            std::vector<vk::SubpassDescription> subpasses = { subpass_deferred, subpass_transparent };

            render_pass_info.dependencyCount = dependencies.size();
            render_pass_info.pDependencies = dependencies.data();
            render_pass_info.subpassCount = subpasses.size();
            render_pass_info.pSubpasses = subpasses.data();

            gbuffer_render_pass = gpu->device->createRenderPassUnique(render_pass_info, nullptr);
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
            output_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

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

        vk::DescriptorSetLayoutBinding raytrace_output_binding_light;
        raytrace_output_binding_light.binding = 0;
        raytrace_output_binding_light.descriptorCount = 1;
        raytrace_output_binding_light.descriptorType = vk::DescriptorType::eStorageImage;
        raytrace_output_binding_light.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

        vk::DescriptorSetLayoutBinding light_binding;
        light_binding.binding = 3;
        light_binding.descriptorCount = 1;
        light_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        light_binding.pImmutableSamplers = nullptr;
        light_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR;

        vk::DescriptorSetLayoutBinding position_sample_layout_binding;
        position_sample_layout_binding.binding = 4;
        position_sample_layout_binding.descriptorCount = 1;
        position_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        position_sample_layout_binding.pImmutableSamplers = nullptr;
        position_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

        //vk::DescriptorSetLayoutBinding normal_sample_layout_binding;
        normal_sample_layout_binding.binding = 5;
        normal_sample_layout_binding.descriptorCount = 1;
        normal_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        normal_sample_layout_binding.pImmutableSamplers = nullptr;
        normal_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

        vk::DescriptorSetLayoutBinding face_normal_sample_layout_binding;
        face_normal_sample_layout_binding.binding = 6;
        face_normal_sample_layout_binding.descriptorCount = 1;
        face_normal_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        face_normal_sample_layout_binding.pImmutableSamplers = nullptr;
        face_normal_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

        vk::DescriptorSetLayoutBinding material_index_sample_layout_binding;
        material_index_sample_layout_binding.binding = 7;
        material_index_sample_layout_binding.descriptorCount = 1;
        material_index_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        material_index_sample_layout_binding.pImmutableSamplers = nullptr;
        material_index_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

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
        mesh_info_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR | vk::ShaderStageFlagBits::eIntersectionKHR;

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
            raytrace_output_binding_light,
            light_binding,
            position_sample_layout_binding,
            normal_sample_layout_binding,
            face_normal_sample_layout_binding,
            material_index_sample_layout_binding
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

        //vk::DescriptorSetLayoutBinding position_sample_layout_binding;
        position_sample_layout_binding.binding = 0;
        position_sample_layout_binding.descriptorCount = 1;
        position_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        position_sample_layout_binding.pImmutableSamplers = nullptr;
        position_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        //vk::DescriptorSetLayoutBinding albedo_sample_layout_binding;
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

        //vk::DescriptorSetLayoutBinding accumulation_sample_layout_binding;
        accumulation_sample_layout_binding.binding = 4;
        accumulation_sample_layout_binding.descriptorCount = 1;
        accumulation_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        accumulation_sample_layout_binding.pImmutableSamplers = nullptr;
        accumulation_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        //vk::DescriptorSetLayoutBinding revealage_sample_layout_binding;
        revealage_sample_layout_binding.binding = 5;
        revealage_sample_layout_binding.descriptorCount = 1;
        revealage_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        revealage_sample_layout_binding.pImmutableSamplers = nullptr;
        revealage_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        //vk::DescriptorSetLayoutBinding mesh_info_binding;
        mesh_info_binding.binding = 6;
        mesh_info_binding.descriptorCount = 1;
        mesh_info_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        mesh_info_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding light_buffer_binding;
        light_buffer_binding.binding = 7;
        light_buffer_binding.descriptorCount = 1;
        light_buffer_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        light_buffer_binding.pImmutableSamplers = nullptr;
        light_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding camera_buffer_binding;
        camera_buffer_binding.binding = 8;
        camera_buffer_binding.descriptorCount = 1;
        camera_buffer_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        camera_buffer_binding.pImmutableSamplers = nullptr;
        camera_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        std::vector<vk::DescriptorSetLayoutBinding> rtx_deferred_bindings = { position_sample_layout_binding, albedo_sample_layout_binding, light_sample_layout_binding,
            material_index_layout_binding, accumulation_sample_layout_binding, revealage_sample_layout_binding, mesh_info_binding, light_buffer_binding, camera_buffer_binding };

        vk::DescriptorSetLayoutCreateInfo rtx_deferred_layout_info;
        rtx_deferred_layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
        rtx_deferred_layout_info.bindingCount = static_cast<uint32_t>(rtx_deferred_bindings.size());
        rtx_deferred_layout_info.pBindings = rtx_deferred_bindings.data();

        rtx_descriptor_layout_deferred = gpu->device->createDescriptorSetLayoutUnique(rtx_deferred_layout_info, nullptr);
    }

    void RendererHybrid::createGraphicsPipeline()
    {
        auto vertex_module = getShader("shaders/gbuffer_vert.spv");
        auto landscape_vertex_module = getShader("shaders/landscape_gbuffer_vert.spv");
        auto particle_vertex_module = getShader("shaders/particle_gbuffer_vert.spv");
        auto fragment_module = getShader("shaders/gbuffer_frag.spv");
        auto particle_fragment_module = getShader("shaders/particle_blend.spv");

        vk::PipelineShaderStageCreateInfo vert_shader_stage_info;
        vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
        vert_shader_stage_info.module = *vertex_module;
        vert_shader_stage_info.pName = "main";

        vk::PipelineShaderStageCreateInfo landscape_vert_shader_stage_info;
        landscape_vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
        landscape_vert_shader_stage_info.module = *landscape_vertex_module;
        landscape_vert_shader_stage_info.pName = "main";

        vk::PipelineShaderStageCreateInfo particle_vert_shader_stage_info;
        particle_vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
        particle_vert_shader_stage_info.module = *particle_vertex_module;
        particle_vert_shader_stage_info.pName = "main";

        vk::PipelineShaderStageCreateInfo frag_shader_stage_info;
        frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
        frag_shader_stage_info.module = *fragment_module;
        frag_shader_stage_info.pName = "main";

        vk::PipelineShaderStageCreateInfo particle_frag_shader_stage_info;
        particle_frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
        particle_frag_shader_stage_info.module = *particle_fragment_module;
        particle_frag_shader_stage_info.pName = "main";

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {vert_shader_stage_info, frag_shader_stage_info};
        std::array<vk::PipelineShaderStageCreateInfo, 2> landscape_shaderStages = {landscape_vert_shader_stage_info, frag_shader_stage_info};
        std::array<vk::PipelineShaderStageCreateInfo, 2> particle_shaderStages = {particle_vert_shader_stage_info, particle_frag_shader_stage_info};

        vk::PipelineVertexInputStateCreateInfo main_vertex_input_info;

        auto main_binding_descriptions = engine->settings.renderer_settings.model_vertex_input_binding_descriptions;
        auto main_attribute_descriptions = engine->settings.renderer_settings.model_vertex_input_attribute_descriptions;

        main_vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(main_binding_descriptions.size());
        main_vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(main_attribute_descriptions.size());
        main_vertex_input_info.pVertexBindingDescriptions = main_binding_descriptions.data();
        main_vertex_input_info.pVertexAttributeDescriptions = main_attribute_descriptions.data();

        vk::PipelineVertexInputStateCreateInfo landscape_vertex_input_info;

        auto landscape_binding_descriptions = engine->settings.renderer_settings.landscape_vertex_input_binding_descriptions;
        auto landscape_attribute_descriptions = engine->settings.renderer_settings.landscape_vertex_input_attribute_descriptions;

        landscape_vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(landscape_binding_descriptions.size());
        landscape_vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(landscape_attribute_descriptions.size());
        landscape_vertex_input_info.pVertexBindingDescriptions = landscape_binding_descriptions.data();
        landscape_vertex_input_info.pVertexAttributeDescriptions = landscape_attribute_descriptions.data();

        vk::PipelineVertexInputStateCreateInfo particle_vertex_input_info;

        auto particle_binding_descriptions = engine->settings.renderer_settings.particle_vertex_input_binding_descriptions;
        auto particle_attribute_descriptions = engine->settings.renderer_settings.particle_vertex_input_attribute_descriptions;

        particle_vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(particle_binding_descriptions.size());
        particle_vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(particle_attribute_descriptions.size());
        particle_vertex_input_info.pVertexBindingDescriptions = particle_binding_descriptions.data();
        particle_vertex_input_info.pVertexAttributeDescriptions = particle_attribute_descriptions.data();

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
        rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
        rasterizer.depthBiasEnable = false;

        vk::PipelineMultisampleStateCreateInfo multisampling;
        multisampling.sampleShadingEnable = false;
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

        vk::PipelineDepthStencilStateCreateInfo depth_stencil;
        depth_stencil.depthTestEnable = true;
        depth_stencil.depthWriteEnable = true;
        depth_stencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
        depth_stencil.depthBoundsTestEnable = false;
        depth_stencil.stencilTestEnable = false;

        vk::PipelineColorBlendAttachmentState color_blend_attachment;
        color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        color_blend_attachment.blendEnable = false;
        color_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;
        color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
        color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcColor;
        color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcColor;
        color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;

        vk::PipelineColorBlendAttachmentState color_blend_attachment_accumulation = color_blend_attachment;
        color_blend_attachment_accumulation.blendEnable = true;
        color_blend_attachment_accumulation.srcColorBlendFactor = vk::BlendFactor::eOne;
        color_blend_attachment_accumulation.dstColorBlendFactor = vk::BlendFactor::eOne;
        color_blend_attachment_accumulation.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        color_blend_attachment_accumulation.dstAlphaBlendFactor = vk::BlendFactor::eOne;
        vk::PipelineColorBlendAttachmentState color_blend_attachment_revealage = color_blend_attachment;
        color_blend_attachment_revealage.blendEnable = true;
        color_blend_attachment_revealage.srcColorBlendFactor = vk::BlendFactor::eZero;
        color_blend_attachment_revealage.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcColor;
        color_blend_attachment_revealage.srcAlphaBlendFactor = vk::BlendFactor::eZero;
        color_blend_attachment_revealage.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcColor;

        std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachment_states(5, color_blend_attachment);
        std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachment_states_subpass1{ color_blend_attachment_accumulation, color_blend_attachment_revealage };

        vk::PipelineColorBlendStateCreateInfo color_blending;
        color_blending.logicOpEnable = false;
        color_blending.logicOp = vk::LogicOp::eCopy;
        color_blending.attachmentCount = static_cast<uint32_t>(color_blend_attachment_states.size());
        color_blending.pAttachments = color_blend_attachment_states.data();
        color_blending.blendConstants[0] = 0.0f;
        color_blending.blendConstants[1] = 0.0f;
        color_blending.blendConstants[2] = 0.0f;
        color_blending.blendConstants[3] = 0.0f;

        vk::PipelineColorBlendStateCreateInfo color_blending_subpass1 = color_blending;
        color_blending_subpass1.attachmentCount = static_cast<uint32_t>(color_blend_attachment_states_subpass1.size());
        color_blending_subpass1.pAttachments = color_blend_attachment_states_subpass1.data();

        std::array<vk::DescriptorSetLayout, 1> descriptor_layouts = { *static_descriptor_set_layout };

        vk::PipelineLayoutCreateInfo pipeline_layout_info;
        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_layouts.size());
        pipeline_layout_info.pSetLayouts = descriptor_layouts.data();

        //material index
        vk::PushConstantRange push_constant_range;
        push_constant_range.stageFlags = vk::ShaderStageFlagBits::eFragment;
        push_constant_range.size = 4;
        push_constant_range.offset = 0;

        pipeline_layout_info.pPushConstantRanges = &push_constant_range;
        pipeline_layout_info.pushConstantRangeCount = 1;

        pipeline_layout = gpu->device->createPipelineLayoutUnique(pipeline_layout_info, nullptr);

        vk::GraphicsPipelineCreateInfo landscape_pipeline_info;
        landscape_pipeline_info.stageCount = static_cast<uint32_t>(landscape_shaderStages.size());
        landscape_pipeline_info.pStages = landscape_shaderStages.data();
        landscape_pipeline_info.pVertexInputState = &landscape_vertex_input_info;
        landscape_pipeline_info.pInputAssemblyState = &input_assembly;
        landscape_pipeline_info.pViewportState = &viewport_state;
        landscape_pipeline_info.pRasterizationState = &rasterizer;
        landscape_pipeline_info.pMultisampleState = &multisampling;
        landscape_pipeline_info.pDepthStencilState = &depth_stencil;
        landscape_pipeline_info.pColorBlendState = &color_blending;
        landscape_pipeline_info.layout = *pipeline_layout;
        landscape_pipeline_info.renderPass = *gbuffer_render_pass;
        landscape_pipeline_info.subpass = 0;
        landscape_pipeline_info.basePipelineHandle = nullptr;

        vk::GraphicsPipelineCreateInfo main_pipeline_info = landscape_pipeline_info;
        main_pipeline_info.stageCount = static_cast<uint32_t>(shaderStages.size());
        main_pipeline_info.pStages = shaderStages.data();
        main_pipeline_info.pVertexInputState = &main_vertex_input_info;

        vk::PipelineDepthStencilStateCreateInfo particle_depth_stencil;
        particle_depth_stencil.depthTestEnable = true;
        particle_depth_stencil.depthWriteEnable = false;
        particle_depth_stencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
        particle_depth_stencil.depthBoundsTestEnable = false;
        particle_depth_stencil.stencilTestEnable = false;

        vk::GraphicsPipelineCreateInfo particle_pipeline_info = landscape_pipeline_info;
        particle_pipeline_info.pDepthStencilState = &particle_depth_stencil;
        particle_pipeline_info.stageCount = static_cast<uint32_t>(particle_shaderStages.size());
        particle_pipeline_info.pStages = particle_shaderStages.data();
        particle_pipeline_info.pVertexInputState = &particle_vertex_input_info;
        particle_pipeline_info.pColorBlendState = &color_blending_subpass1;
        particle_pipeline_info.subpass = 1;

        main_pipeline_group.graphics_pipeline = gpu->device->createGraphicsPipelineUnique(nullptr, main_pipeline_info, nullptr);
        landscape_pipeline_group.graphics_pipeline = gpu->device->createGraphicsPipelineUnique(nullptr, landscape_pipeline_info, nullptr);
        particle_pipeline_group.graphics_pipeline = gpu->device->createGraphicsPipelineUnique(nullptr, particle_pipeline_info, nullptr);

        fragment_module = getShader("shaders/blend.spv");

        frag_shader_stage_info.module = *fragment_module;

        shaderStages[1] = frag_shader_stage_info;
        landscape_shaderStages[1] = frag_shader_stage_info;

        main_pipeline_group.blended_graphics_pipeline = gpu->device->createGraphicsPipelineUnique(nullptr, main_pipeline_info, nullptr);
        landscape_pipeline_group.blended_graphics_pipeline = gpu->device->createGraphicsPipelineUnique(nullptr, landscape_pipeline_info, nullptr);
        particle_pipeline_group.blended_graphics_pipeline = gpu->device->createGraphicsPipelineUnique(nullptr, particle_pipeline_info, nullptr);

        landscape_vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(1);
        landscape_vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(4);

        vertex_module = getShader("shaders/deferred.spv");
        vert_shader_stage_info.module = *vertex_module;

        fragment_module = getShader("shaders/deferred_raytrace_hybrid.spv");
        frag_shader_stage_info.module = *fragment_module;

        landscape_shaderStages[0] = vert_shader_stage_info;
        landscape_shaderStages[1] = frag_shader_stage_info;
        vk::GraphicsPipelineCreateInfo deferred_pipeline_info = landscape_pipeline_info;

        deferred_pipeline_info.renderPass = *render_pass;
        color_blend_attachment_states = std::vector<vk::PipelineColorBlendAttachmentState>(1, color_blend_attachment);
        color_blending.attachmentCount = static_cast<uint32_t>(color_blend_attachment_states.size());
        color_blending.pAttachments = color_blend_attachment_states.data();

        std::array<vk::DescriptorSetLayout, 1> deferred_descriptor_layouts = { *deferred_descriptor_set_layout };

        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(deferred_descriptor_layouts.size());
        pipeline_layout_info.pSetLayouts = deferred_descriptor_layouts.data();

        {
            //ray-tracing pipeline
            vk::UniqueShaderModule raygen_shader_module = getShader("shaders/raygen_hybrid.spv");
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

    void RendererHybrid::createCommandPool()
    {
        vk::CommandPoolCreateInfo pool_info = {};
        pool_info.queueFamilyIndex = gpu->graphics_queue_index;

        command_pool = gpu->device->createCommandPoolUnique(pool_info, nullptr);
    }

    void RendererHybrid::createGBufferResources()
    {
        gbuffer.position.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.normal.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.face_normal.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.albedo.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.accumulation.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR16G16B16A16Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.revealage.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR16Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.material.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR16Uint, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.depth.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, gpu->getDepthFormat(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = gbuffer.position.image->image;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = vk::Format::eR32G32B32A32Sfloat;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;
        gbuffer.position.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = gbuffer.normal.image->image;
        gbuffer.normal.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = gbuffer.face_normal.image->image;
        gbuffer.face_normal.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = gbuffer.albedo.image->image;
        image_view_info.format = vk::Format::eR8G8B8A8Unorm;
        gbuffer.albedo.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = gbuffer.accumulation.image->image;
        image_view_info.format = vk::Format::eR16G16B16A16Sfloat;
        gbuffer.accumulation.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = gbuffer.revealage.image->image;
        image_view_info.format = vk::Format::eR16Sfloat;
        gbuffer.revealage.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = gbuffer.material.image->image;
        image_view_info.format = vk::Format::eR16Uint;
        gbuffer.material.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);
        image_view_info.image = gbuffer.depth.image->image;
        image_view_info.format = gpu->getDepthFormat();
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        gbuffer.depth.image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);

        std::vector<vk::ImageView> attachments = { *gbuffer.position.image_view, *gbuffer.normal.image_view, *gbuffer.face_normal.image_view,
            *gbuffer.albedo.image_view, *gbuffer.accumulation.image_view, *gbuffer.revealage.image_view, *gbuffer.material.image_view, *gbuffer.depth.image_view };

        vk::FramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.renderPass = *gbuffer_render_pass;
        framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = swapchain->extent.width;
        framebuffer_info.height = swapchain->extent.height;
        framebuffer_info.layers = 1;

        gbuffer.frame_buffer = gpu->device->createFramebufferUnique(framebuffer_info, nullptr);

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

        rtx_gbuffer.albedo.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);
        rtx_gbuffer.light.image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);

        //vk::ImageViewCreateInfo image_view_info;
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

        //vk::SamplerCreateInfo sampler_info = {};
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

    void RendererHybrid::createDeferredCommandBuffer()
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.commandPool = *command_pool;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = getImageCount();

        deferred_command_buffers = gpu->device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info);

        for (int i = 0; i < deferred_command_buffers.size(); ++i)
        {
            vk::CommandBuffer buffer = *deferred_command_buffers[i];

            vk::CommandBufferBeginInfo begin_info = {};
            begin_info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

            buffer.begin(begin_info);

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
            albedo_info.imageView = *gbuffer.albedo.image_view;
            albedo_info.sampler = *gbuffer.sampler;

            vk::DescriptorImageInfo light_info;
            light_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            light_info.imageView = *rtx_gbuffer.light.image_view;
            light_info.sampler = *rtx_gbuffer.sampler;

            vk::DescriptorImageInfo position_info;
            position_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            position_info.imageView = *gbuffer.position.image_view;
            position_info.sampler = *gbuffer.sampler;

            vk::DescriptorImageInfo deferred_material_index_info;
            deferred_material_index_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            deferred_material_index_info.imageView = *gbuffer.material.image_view;
            deferred_material_index_info.sampler = *gbuffer.sampler;

            vk::DescriptorImageInfo accumulation_info;
            accumulation_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            accumulation_info.imageView = *gbuffer.accumulation.image_view;
            accumulation_info.sampler = *gbuffer.sampler;

            vk::DescriptorImageInfo revealage_info;
            revealage_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            revealage_info.imageView = *gbuffer.revealage.image_view;
            revealage_info.sampler = *gbuffer.sampler;

            vk::DescriptorBufferInfo mesh_info;
            mesh_info.buffer = mesh_info_buffer->buffer;
            mesh_info.offset = sizeof(RendererHybrid::MeshInfo) * max_acceleration_binding_index * i;
            mesh_info.range = sizeof(RendererHybrid::MeshInfo) * max_acceleration_binding_index;

            vk::DescriptorBufferInfo light_buffer_info;
            light_buffer_info.buffer = engine->lights->light_buffer->buffer;
            light_buffer_info.offset = i * uniform_buffer_align_up(sizeof(engine->lights->light));
            light_buffer_info.range = sizeof(engine->lights->light);

            vk::DescriptorBufferInfo camera_buffer_info;
            camera_buffer_info.buffer = camera_buffers.view_proj_ubo->buffer;
            camera_buffer_info.offset = i * uniform_buffer_align_up(sizeof(Camera::CameraData));
            camera_buffer_info.range = sizeof(Camera::CameraData);

            std::vector<vk::WriteDescriptorSet> descriptorWrites {9};

            descriptorWrites[0].dstSet = nullptr;
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &position_info;

            descriptorWrites[1].dstSet = nullptr;
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &albedo_info;

            descriptorWrites[2].dstSet = nullptr;
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo = &light_info;

            descriptorWrites[3].dstSet = nullptr;
            descriptorWrites[3].dstBinding = 3;
            descriptorWrites[3].dstArrayElement = 0;
            descriptorWrites[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pImageInfo = &deferred_material_index_info;

            descriptorWrites[4].dstSet = nullptr;
            descriptorWrites[4].dstBinding = 4;
            descriptorWrites[4].dstArrayElement = 0;
            descriptorWrites[4].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[4].descriptorCount = 1;
            descriptorWrites[4].pImageInfo = &accumulation_info;

            descriptorWrites[5].dstSet = nullptr;
            descriptorWrites[5].dstBinding = 5;
            descriptorWrites[5].dstArrayElement = 0;
            descriptorWrites[5].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[5].descriptorCount = 1;
            descriptorWrites[5].pImageInfo = &revealage_info;

            descriptorWrites[6].dstSet = nullptr;
            descriptorWrites[6].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[6].dstBinding = 6;
            descriptorWrites[6].dstArrayElement = 0;
            descriptorWrites[6].descriptorCount = 1;
            descriptorWrites[6].pBufferInfo = &mesh_info;

            descriptorWrites[7].dstSet = nullptr;
            descriptorWrites[7].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[7].dstBinding = 7;
            descriptorWrites[7].dstArrayElement = 0;
            descriptorWrites[7].descriptorCount = 1;
            descriptorWrites[7].pBufferInfo = &light_buffer_info;

            descriptorWrites[8].dstSet = nullptr;
            descriptorWrites[8].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[8].dstBinding = 8;
            descriptorWrites[8].dstArrayElement = 0;
            descriptorWrites[8].descriptorCount = 1;
            descriptorWrites[8].pBufferInfo = &camera_buffer_info;

            buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *rtx_deferred_pipeline_layout, 0, descriptorWrites);

            buffer.draw(3, 1, 0, 0);

            buffer.endRenderPass();

            buffer.end();
        }
    }

    void RendererHybrid::initializeCameraBuffers()
    {
        camera_buffers.view_proj_ubo = engine->renderer->gpu->memory_manager->GetBuffer(engine->renderer->uniform_buffer_align_up(sizeof(Camera::CameraData)) * getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        camera_buffers.view_proj_mapped = static_cast<uint8_t*>(camera_buffers.view_proj_ubo->map(0, engine->renderer->uniform_buffer_align_up(sizeof(Camera::CameraData)) * getImageCount(), {}));
    }

    vk::CommandBuffer RendererHybrid::getRenderCommandbuffer(uint32_t image_index)
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.commandPool = *command_pool;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = 1;

        auto buffer = gpu->device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        buffer[0]->begin(begin_info);

        vk::RenderPassBeginInfo renderpass_info = {};
        renderpass_info.renderArea.offset = vk::Offset2D{ 0, 0 };

        renderpass_info.renderPass = *gbuffer_render_pass;
        renderpass_info.framebuffer = *gbuffer.frame_buffer;
        std::array<vk::ClearValue, 8> clearValues = {};
        clearValues[0].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
        clearValues[1].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
        clearValues[2].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
        clearValues[3].color = std::array<float, 4>{ 0.2f, 0.4f, 0.6f, 1.0f };
        clearValues[4].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
        clearValues[5].color = std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f };
        clearValues[6].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
        clearValues[7].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

        renderpass_info.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderpass_info.pClearValues = clearValues.data();
        renderpass_info.renderArea.extent = swapchain->extent;

        buffer[0]->beginRenderPass(renderpass_info, vk::SubpassContents::eSecondaryCommandBuffers);
        auto secondary_buffers = engine->worker_pool->getSecondaryGraphicsBuffers(image_index);
        if (!secondary_buffers.empty())
            buffer[0]->executeCommands(secondary_buffers);
        buffer[0]->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
        auto particle_buffers = engine->worker_pool->getParticleGraphicsBuffers(image_index);
        if (!particle_buffers.empty())
            buffer[0]->executeCommands(particle_buffers);
        buffer[0]->endRenderPass();

        buffer[0]->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline);

        buffer[0]->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline_layout, 0, *rtx_descriptor_sets_const[image_index], {});

        vk::WriteDescriptorSet write_info_target_light;
        write_info_target_light.descriptorCount = 1;
        write_info_target_light.descriptorType = vk::DescriptorType::eStorageImage;
        write_info_target_light.dstBinding = 0;
        write_info_target_light.dstArrayElement = 0;

        vk::DescriptorImageInfo target_image_info_light;
        target_image_info_light.imageView = *rtx_gbuffer.light.image_view;
        target_image_info_light.imageLayout = vk::ImageLayout::eGeneral;
        write_info_target_light.pImageInfo = &target_image_info_light;

        vk::DescriptorBufferInfo light_buffer_info;
        light_buffer_info.buffer = engine->lights->light_buffer->buffer;
        light_buffer_info.offset = getCurrentImage() * uniform_buffer_align_up(sizeof(engine->lights->light));
        light_buffer_info.range = sizeof(engine->lights->light);

        vk::WriteDescriptorSet write_info_light;
        write_info_light.descriptorCount = 1;
        write_info_light.descriptorType = vk::DescriptorType::eUniformBuffer;
        write_info_light.dstBinding = 3;
        write_info_light.dstArrayElement = 0;
        write_info_light.pBufferInfo = &light_buffer_info;

        vk::DescriptorImageInfo position_info;
        position_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        position_info.imageView = *gbuffer.position.image_view;
        position_info.sampler = *gbuffer.sampler;

        vk::WriteDescriptorSet write_info_position;
        write_info_position.descriptorCount = 1;
        write_info_position.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write_info_position.dstBinding = 4;
        write_info_position.dstArrayElement = 0;
        write_info_position.pImageInfo = &position_info;

        vk::DescriptorImageInfo normal_info;
        normal_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        normal_info.imageView = *gbuffer.normal.image_view;
        normal_info.sampler = *gbuffer.sampler;

        vk::WriteDescriptorSet write_info_normal;
        write_info_normal.descriptorCount = 1;
        write_info_normal.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write_info_normal.dstBinding = 5;
        write_info_normal.dstArrayElement = 0;
        write_info_normal.pImageInfo = &normal_info;

        vk::DescriptorImageInfo face_normal_info;
        face_normal_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        face_normal_info.imageView = *gbuffer.face_normal.image_view;
        face_normal_info.sampler = *gbuffer.sampler;

        vk::WriteDescriptorSet write_info_face_normal;
        write_info_face_normal.descriptorCount = 1;
        write_info_face_normal.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write_info_face_normal.dstBinding = 6;
        write_info_face_normal.dstArrayElement = 0;
        write_info_face_normal.pImageInfo = &face_normal_info;

        vk::DescriptorImageInfo material_index_info;
        material_index_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        material_index_info.imageView = *gbuffer.material.image_view;
        material_index_info.sampler = *gbuffer.sampler;

        vk::WriteDescriptorSet write_info_material_index;
        write_info_material_index.descriptorCount = 1;
        write_info_material_index.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write_info_material_index.dstBinding = 7;
        write_info_material_index.dstArrayElement = 0;
        write_info_material_index.pImageInfo = &material_index_info;

        buffer[0]->pushDescriptorSetKHR(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline_layout, 1, { write_info_target_light, write_info_light, write_info_position, write_info_normal, write_info_face_normal, write_info_material_index });

        buffer[0]->traceRaysKHR(raygenSBT, missSBT, hitSBT, {}, swapchain->extent.width, swapchain->extent.height, 1);

        buffer[0]->end();
        render_commandbuffers[image_index] = std::move(buffer[0]);
        return *render_commandbuffers[image_index];
    }

    void RendererHybrid::drawFrame()
    {
        if (!engine->game || !engine->game->scene)
            return;

        engine->worker_pool->deleteFinished();
        gpu->device->waitForFences(*frame_fences[current_frame], true, std::numeric_limits<uint64_t>::max());

        auto prev_image = current_image;
        try
        {
            current_image = gpu->device->acquireNextImageKHR(*swapchain->swapchain, std::numeric_limits<uint64_t>::max(), *image_ready_sem[current_frame], nullptr);
        }
        catch (vk::OutOfDateKHRError&)
        {
            resizeRenderer();
            return;
        }

        engine->worker_pool->clearProcessed(current_image);
        swapchain->checkOldSwapchain(current_image);
        engine->worker_pool->waitIdle();
        if (raytracer->hasQueries())
        {
            raytracer->runQueries(prev_image);
        }
        engine->game->scene->render();

        engine->worker_pool->waitIdle();
        engine->worker_pool->startProcessing(current_image);
        engine->camera->updateBuffers(camera_buffers.view_proj_mapped);
        engine->lights->UpdateLightBuffer();

        std::vector<vk::Semaphore> waitSemaphores = { *image_ready_sem[current_frame]};
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

        std::vector<vk::Semaphore> gbuffer_semaphores = { *gbuffer_sem };
        submitInfo.pSignalSemaphores = gbuffer_semaphores.data();
        submitInfo.signalSemaphoreCount = gbuffer_semaphores.size();

        gpu->graphics_queue.submit(submitInfo, nullptr);

        submitInfo.waitSemaphoreCount = gbuffer_semaphores.size();
        submitInfo.pWaitSemaphores = gbuffer_semaphores.data();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &*deferred_command_buffers[current_image];

        std::vector<vk::Semaphore> signalSemaphores = { *frame_finish_sem[current_frame] };
        submitInfo.signalSemaphoreCount = signalSemaphores.size();
        submitInfo.pSignalSemaphores = signalSemaphores.data();

        gpu->device->resetFences(*frame_fences[current_frame]);

        gpu->graphics_queue.submit(submitInfo, *frame_fences[current_frame]);

        vk::PresentInfoKHR presentInfo = {};

        presentInfo.waitSemaphoreCount = signalSemaphores.size();
        presentInfo.pWaitSemaphores = signalSemaphores.data();

        std::vector<vk::SwapchainKHR> swap_chains = {*swapchain->swapchain};
        presentInfo.swapchainCount = swap_chains.size();
        presentInfo.pSwapchains = swap_chains.data();

        presentInfo.pImageIndices = &current_image;

        try
        {
            gpu->present_queue.presentKHR(presentInfo);
        }
        catch (vk::OutOfDateKHRError&)
        {
            resize = false;
            resizeRenderer();
        }

        if (resize)
        {
            resize = false;
            resizeRenderer();
        }

        current_frame = (current_frame + 1) % max_pending_frames;
    }

    void RendererHybrid::populateAccelerationStructure(TopLevelAccelerationStructure* tlas, BottomLevelAccelerationStructure* blas, const glm::mat3x4& mat, uint64_t resource_index, uint32_t mask, uint32_t shader_binding_offset)
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

    void RendererHybrid::initEntity(EntityInitializer* initializer, WorkerThread* thread)
    {
        initializer->initEntity(this, thread);
    }

    void RendererHybrid::drawEntity(EntityInitializer* initializer, WorkerThread* thread)
    {
        initializer->drawEntity(this, thread);
    }
}
