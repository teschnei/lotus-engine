#include "renderer_rasterization.h"
#include <glm/glm.hpp>
#include <fstream>
#include <iostream>

#include "engine/core.h"
#include "engine/game.h"
#include "engine/config.h"
#include "engine/entity/camera.h"
#include "engine/entity/renderable_entity.h"

namespace lotus
{
    RendererRasterization::RendererRasterization(Engine* _engine) : Renderer(_engine)
    {
    }

    RendererRasterization::~RendererRasterization()
    {
        gpu->device->waitIdle();
        if (camera_buffers.view_proj_ubo)
            camera_buffers.view_proj_ubo->unmap();
        if (camera_buffers.cascade_data_ubo)
            camera_buffers.cascade_data_ubo->unmap();
    }

    Task<> RendererRasterization::Init()
    {
        createRenderpasses();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createDepthImage();
        createFramebuffers();
        createSyncs();
        createCommandPool();
        createShadowmapResources();
        createGBufferResources();
        createAnimationResources();

        initializeCameraBuffers();
        generateCommandBuffers();

        render_commandbuffers.resize(getImageCount());
        co_return;
    }

    void RendererRasterization::generateCommandBuffers()
    {
        createDeferredCommandBuffer();
    }

    void RendererRasterization::updateCameraBuffers()
    {
        engine->camera->updateBuffers(camera_buffers.view_proj_mapped);
        memcpy(camera_buffers.cascade_data_mapped + (getCurrentImage() * uniform_buffer_align_up(sizeof(cascade_data))), &cascade_data, sizeof(cascade_data));
    }

    void RendererRasterization::createRenderpasses()
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

        depth_attachment.storeOp = vk::AttachmentStoreOp::eStore;
        depth_attachment_ref.attachment = 0;
        depth_attachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        std::array<vk::AttachmentDescription, 1> shadow_attachments = { depth_attachment };
        subpass.colorAttachmentCount = 0;
        subpass.pColorAttachments = nullptr;
        render_pass_info.attachmentCount = static_cast<uint32_t>(shadow_attachments.size());
        render_pass_info.pAttachments = shadow_attachments.data();

        vk::SubpassDependency shadowmap_dep1;
        shadowmap_dep1.srcSubpass = VK_SUBPASS_EXTERNAL;
        shadowmap_dep1.dstSubpass = 0;
        shadowmap_dep1.srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;
        shadowmap_dep1.dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;
        shadowmap_dep1.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        shadowmap_dep1.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        shadowmap_dep1.dependencyFlags = vk::DependencyFlagBits::eByRegion;

        vk::SubpassDependency shadowmap_dep2;
        shadowmap_dep2.srcSubpass = 0;
        shadowmap_dep2.dstSubpass = VK_SUBPASS_EXTERNAL;
        shadowmap_dep2.srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests;
        shadowmap_dep2.dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
        shadowmap_dep2.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        shadowmap_dep2.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        shadowmap_dep2.dependencyFlags = vk::DependencyFlagBits::eByRegion;

        std::array<vk::SubpassDependency, 2> shadowmap_subpass_deps = { shadowmap_dep1, shadowmap_dep2 };

        render_pass_info.dependencyCount = 2;
        render_pass_info.pDependencies = shadowmap_subpass_deps.data();

        shadowmap_subpass_deps[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
        shadowmap_subpass_deps[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        shadowmap_subpass_deps[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
        shadowmap_subpass_deps[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

        shadowmap_subpass_deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        shadowmap_subpass_deps[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
        shadowmap_subpass_deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        shadowmap_subpass_deps[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;

        shadowmap_render_pass = gpu->device->createRenderPassUnique(render_pass_info, nullptr);

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

        vk::AttachmentDescription gbuffer_depth_attachment;
        gbuffer_depth_attachment.format = gpu->getDepthFormat();
        gbuffer_depth_attachment.samples = vk::SampleCountFlagBits::e1;
        gbuffer_depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
        gbuffer_depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
        gbuffer_depth_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        gbuffer_depth_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        gbuffer_depth_attachment.initialLayout = vk::ImageLayout::eUndefined;
        gbuffer_depth_attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        std::vector<vk::AttachmentDescription> gbuffer_attachments = { desc_pos, desc_normal, desc_face_normal, desc_albedo, desc_accumulation, desc_revealage, desc_material, gbuffer_depth_attachment };

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

    void RendererRasterization::createDescriptorSetLayout()
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

        vk::DescriptorSetLayoutBinding material_info_layout_binding;
        material_info_layout_binding.binding = 3;
        material_info_layout_binding.descriptorCount = 1;
        material_info_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        material_info_layout_binding.pImmutableSamplers = nullptr;
        material_info_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding mesh_info_layout_binding;
        mesh_info_layout_binding.binding = 4;
        mesh_info_layout_binding.descriptorCount = 1;
        mesh_info_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        mesh_info_layout_binding.pImmutableSamplers = nullptr;
        mesh_info_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        std::vector<vk::DescriptorSetLayoutBinding> static_bindings = { camera_layout_binding, model_layout_binding, sample_layout_binding, material_info_layout_binding, mesh_info_layout_binding };

        vk::DescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
        layout_info.bindingCount = static_cast<uint32_t>(static_bindings.size());
        layout_info.pBindings = static_bindings.data();

        static_descriptor_set_layout = gpu->device->createDescriptorSetLayoutUnique(layout_info, nullptr);

        vk::DescriptorSetLayoutBinding cascade_matrices;
        cascade_matrices.binding = 3;
        cascade_matrices.descriptorCount = 1;
        cascade_matrices.descriptorType = vk::DescriptorType::eUniformBuffer;
        cascade_matrices.pImmutableSamplers = nullptr;
        cascade_matrices.stageFlags = vk::ShaderStageFlagBits::eVertex;

        std::vector<vk::DescriptorSetLayoutBinding> shadowmap_bindings = { camera_layout_binding, model_layout_binding, cascade_matrices, sample_layout_binding };

        layout_info.bindingCount = static_cast<uint32_t>(shadowmap_bindings.size());
        layout_info.pBindings = shadowmap_bindings.data();

        shadowmap_descriptor_set_layout = gpu->device->createDescriptorSetLayoutUnique(layout_info, nullptr);

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

        vk::DescriptorSetLayoutBinding lightsource_deferred_layout_binding;
        lightsource_deferred_layout_binding.binding = 8;
        lightsource_deferred_layout_binding.descriptorCount = 1;
        lightsource_deferred_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        lightsource_deferred_layout_binding.pImmutableSamplers = nullptr;
        lightsource_deferred_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding shadowmap_deferred_layout_binding;
        shadowmap_deferred_layout_binding.binding = 9;
        shadowmap_deferred_layout_binding.descriptorCount = 1;
        shadowmap_deferred_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        shadowmap_deferred_layout_binding.pImmutableSamplers = nullptr;
        shadowmap_deferred_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        //vk::DescriptorSetLayoutBinding material_info_layout_binding;
        material_info_layout_binding.binding = 10;
        material_info_layout_binding.descriptorCount = GlobalResources::max_resource_index;
        material_info_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        material_info_layout_binding.pImmutableSamplers = nullptr;
        material_info_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        //vk::DescriptorSetLayoutBinding mesh_info_layout_binding;
        mesh_info_layout_binding.binding = 11;
        mesh_info_layout_binding.descriptorCount = 1;
        mesh_info_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        mesh_info_layout_binding.pImmutableSamplers = nullptr;
        mesh_info_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        std::vector<vk::DescriptorSetLayoutBinding> deferred_bindings = {
            pos_sample_layout_binding, 
            normal_sample_layout_binding, 
            albedo_sample_layout_binding,
            material_sample_layout_binding,
            accumulation_sample_layout_binding,
            revealage_sample_layout_binding,
            camera_deferred_layout_binding,
            light_deferred_layout_binding,
            lightsource_deferred_layout_binding,
            shadowmap_deferred_layout_binding,
            material_info_layout_binding,
            mesh_info_layout_binding
        };

        layout_info.flags = {};
        layout_info.bindingCount = static_cast<uint32_t>(deferred_bindings.size());
        layout_info.pBindings = deferred_bindings.data();
        std::vector<vk::DescriptorBindingFlags> deferred_binding_flags{ {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, vk::DescriptorBindingFlagBits::ePartiallyBound, {} };
        vk::DescriptorSetLayoutBindingFlagsCreateInfo layout_flags_deferred{ static_cast<uint32_t>(deferred_binding_flags.size()), deferred_binding_flags.data() };
        layout_info.pNext = &layout_flags_deferred;

        deferred_descriptor_set_layout = gpu->device->createDescriptorSetLayoutUnique(layout_info, nullptr);

        std::vector<vk::DescriptorPoolSize> pool_sizes_deferred;
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eUniformBuffer, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eUniformBuffer, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eUniformBuffer, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eUniformBuffer, GlobalResources::max_resource_index);
        pool_sizes_deferred.emplace_back(vk::DescriptorType::eUniformBuffer, 1);

        vk::DescriptorPoolCreateInfo pool_ci;
        pool_ci.maxSets = 3;
        pool_ci.poolSizeCount = static_cast<uint32_t>(pool_sizes_deferred.size());
        pool_ci.pPoolSizes = pool_sizes_deferred.data();
        pool_ci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        deferred_descriptor_pool = gpu->device->createDescriptorPoolUnique(pool_ci, nullptr);

        std::array<vk::DescriptorSetLayout, 3> layouts = { *deferred_descriptor_set_layout, *deferred_descriptor_set_layout, *deferred_descriptor_set_layout };

        vk::DescriptorSetAllocateInfo set_ci;
        set_ci.descriptorPool = *deferred_descriptor_pool;
        set_ci.descriptorSetCount = 3;
        set_ci.pSetLayouts = layouts.data();
        deferred_descriptor_set = gpu->device->allocateDescriptorSetsUnique(set_ci);
    }

    void RendererRasterization::createGraphicsPipeline()
    {
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

        auto vertex_module = getShader("shaders/deferred.spv");
        vk::PipelineShaderStageCreateInfo vert_shader_stage_info;
        vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
        vert_shader_stage_info.module = *vertex_module;
        vert_shader_stage_info.pName = "main";

        auto fragment_module = getShader("shaders/deferred_raster.spv");
        vk::PipelineShaderStageCreateInfo frag_shader_stage_info;
        frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
        frag_shader_stage_info.module = *fragment_module;
        frag_shader_stage_info.pName = "main";

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vert_shader_stage_info, frag_shader_stage_info };

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
        pipeline_info.renderPass = *render_pass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = nullptr;

        std::array<vk::DescriptorSetLayout, 1> deferred_descriptor_layouts = { *deferred_descriptor_set_layout };

        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(deferred_descriptor_layouts.size());
        pipeline_layout_info.pSetLayouts = deferred_descriptor_layouts.data();

        deferred_pipeline_layout = gpu->device->createPipelineLayoutUnique(pipeline_layout_info, nullptr);

        pipeline_info.layout = *deferred_pipeline_layout;

        deferred_pipeline = gpu->device->createGraphicsPipelineUnique(nullptr, pipeline_info, nullptr);

        std::array<vk::DescriptorSetLayout, 1> shadowmap_descriptor_layouts = { *shadowmap_descriptor_set_layout };

        vk::PushConstantRange push_constant_range_shadowmap;
        push_constant_range_shadowmap.stageFlags = vk::ShaderStageFlagBits::eVertex;
        push_constant_range_shadowmap.size = sizeof(uint32_t);
        push_constant_range_shadowmap.offset = 4;

        std::vector<vk::PushConstantRange> push_constant_ranges = { push_constant_range, push_constant_range_shadowmap };

        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(shadowmap_descriptor_layouts.size());
        pipeline_layout_info.pSetLayouts = shadowmap_descriptor_layouts.data();
        pipeline_layout_info.pushConstantRangeCount = push_constant_ranges.size();
        pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();

        shadowmap_pipeline_layout = gpu->device->createPipelineLayoutUnique(pipeline_layout_info, nullptr);
    }

    void RendererRasterization::createDepthImage()
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

    void RendererRasterization::createFramebuffers()
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

    void RendererRasterization::createSyncs()
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

    void RendererRasterization::createShadowmapResources()
    {
        auto format = gpu->getDepthFormat();

        shadowmap_image = gpu->memory_manager->GetImage(engine->settings.renderer_settings.shadowmap_dimension, engine->settings.renderer_settings.shadowmap_dimension, format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, shadowmap_cascades);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = shadowmap_image->image;
        image_view_info.viewType = vk::ImageViewType::e2DArray;
        image_view_info.format = format;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = shadowmap_cascades;
        shadowmap_image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);

        for (uint32_t i = 0; i < shadowmap_cascades; ++i)
        {
            ShadowmapCascade& cascade = cascades[i];
            image_view_info.subresourceRange.baseArrayLayer = i;
            image_view_info.subresourceRange.layerCount = 1;

            cascade.shadowmap_image_view = gpu->device->createImageViewUnique(image_view_info, nullptr);

            std::array<vk::ImageView, 1> attachments = {
                *cascade.shadowmap_image_view
            };

            vk::FramebufferCreateInfo framebuffer_info = {};
            framebuffer_info.renderPass = *shadowmap_render_pass;
            framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebuffer_info.pAttachments = attachments.data();
            framebuffer_info.width = engine->settings.renderer_settings.shadowmap_dimension;
            framebuffer_info.height = engine->settings.renderer_settings.shadowmap_dimension;
            framebuffer_info.layers = 1;

            cascade.shadowmap_frame_buffer = gpu->device->createFramebufferUnique(framebuffer_info, nullptr);
        }

        vk::SamplerCreateInfo sampler_info = {};
        sampler_info.magFilter = vk::Filter::eLinear;
        sampler_info.minFilter = vk::Filter::eLinear;
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

        shadowmap_sampler = gpu->device->createSamplerUnique(sampler_info, nullptr);
    }

    void RendererRasterization::createGBufferResources()
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
    }

    void RendererRasterization::createDeferredCommandBuffer()
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

            std::array<vk::ClearValue, 2> clearValues;
            clearValues[0].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
            clearValues[1].depthStencil = vk::ClearDepthStencilValue{ 1.f, 0 };

            vk::RenderPassBeginInfo renderpass_info;
            renderpass_info.renderPass = *render_pass;
            renderpass_info.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderpass_info.pClearValues = clearValues.data();
            renderpass_info.renderArea.offset = vk::Offset2D{ 0, 0 };
            renderpass_info.renderArea.extent = swapchain->extent;
            renderpass_info.framebuffer = *frame_buffers[i];
            buffer.beginRenderPass(renderpass_info, vk::SubpassContents::eInline);

            vk::DescriptorImageInfo pos_info;
            pos_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            pos_info.imageView = *gbuffer.position.image_view;
            pos_info.sampler = *gbuffer.sampler;

            vk::DescriptorImageInfo normal_info;
            normal_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            normal_info.imageView = *gbuffer.normal.image_view;
            normal_info.sampler = *gbuffer.sampler;

            vk::DescriptorImageInfo albedo_info;
            albedo_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            albedo_info.imageView = *gbuffer.albedo.image_view;
            albedo_info.sampler = *gbuffer.sampler;

            vk::DescriptorImageInfo material_info;
            material_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            material_info.imageView = *gbuffer.material.image_view;
            material_info.sampler = *gbuffer.sampler;

            vk::DescriptorImageInfo accumulation_info;
            accumulation_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            accumulation_info.imageView = *gbuffer.accumulation.image_view;
            accumulation_info.sampler = *gbuffer.sampler;

            vk::DescriptorImageInfo revealage_info;
            revealage_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            revealage_info.imageView = *gbuffer.revealage.image_view;
            revealage_info.sampler = *gbuffer.sampler;

            vk::DescriptorBufferInfo camera_buffer_info;
            camera_buffer_info.buffer = camera_buffers.view_proj_ubo->buffer;
            camera_buffer_info.offset = i * uniform_buffer_align_up(sizeof(Camera::CameraData));
            camera_buffer_info.range = sizeof(Camera::CameraData);

            std::vector<vk::WriteDescriptorSet> descriptorWrites{ 11 };

            descriptorWrites[0].dstSet = *deferred_descriptor_set[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &pos_info;

            descriptorWrites[1].dstSet = *deferred_descriptor_set[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &normal_info;

            descriptorWrites[2].dstSet = *deferred_descriptor_set[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo = &albedo_info;

            descriptorWrites[3].dstSet = *deferred_descriptor_set[i];
            descriptorWrites[3].dstBinding = 3;
            descriptorWrites[3].dstArrayElement = 0;
            descriptorWrites[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pImageInfo = &material_info;

            descriptorWrites[4].dstSet = *deferred_descriptor_set[i];
            descriptorWrites[4].dstBinding = 4;
            descriptorWrites[4].dstArrayElement = 0;
            descriptorWrites[4].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[4].descriptorCount = 1;
            descriptorWrites[4].pImageInfo = &accumulation_info;

            descriptorWrites[5].dstSet = *deferred_descriptor_set[i];
            descriptorWrites[5].dstBinding = 5;
            descriptorWrites[5].dstArrayElement = 0;
            descriptorWrites[5].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[5].descriptorCount = 1;
            descriptorWrites[5].pImageInfo = &revealage_info;

            descriptorWrites[6].dstSet = *deferred_descriptor_set[i];
            descriptorWrites[6].dstBinding = 6;
            descriptorWrites[6].dstArrayElement = 0;
            descriptorWrites[6].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[6].descriptorCount = 1;
            descriptorWrites[6].pBufferInfo = &camera_buffer_info;

            vk::DescriptorBufferInfo light_buffer_info;
            light_buffer_info.buffer = engine->lights->light_buffer->buffer;
            light_buffer_info.offset = i * engine->lights->GetBufferSize();
            light_buffer_info.range = engine->lights->GetBufferSize();

            vk::DescriptorImageInfo shadowmap_image_info;
            shadowmap_image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            shadowmap_image_info.imageView = *shadowmap_image_view;
            shadowmap_image_info.sampler = *shadowmap_sampler;

            vk::DescriptorBufferInfo cascade_buffer_info;
            cascade_buffer_info.buffer = camera_buffers.cascade_data_ubo->buffer;
            cascade_buffer_info.offset = i * uniform_buffer_align_up(sizeof(cascade_data));
            cascade_buffer_info.range = sizeof(cascade_data);

            descriptorWrites[7].dstSet = *deferred_descriptor_set[i];
            descriptorWrites[7].dstBinding = 7;
            descriptorWrites[7].dstArrayElement = 0;
            descriptorWrites[7].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[7].descriptorCount = 1;
            descriptorWrites[7].pBufferInfo = &light_buffer_info;

            descriptorWrites[8].dstSet = *deferred_descriptor_set[i];
            descriptorWrites[8].dstBinding = 8;
            descriptorWrites[8].dstArrayElement = 0;
            descriptorWrites[8].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[8].descriptorCount = 1;
            descriptorWrites[8].pBufferInfo = &cascade_buffer_info;

            descriptorWrites[9].dstSet = *deferred_descriptor_set[i];
            descriptorWrites[9].dstBinding = 9;
            descriptorWrites[9].dstArrayElement = 0;
            descriptorWrites[9].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[9].descriptorCount = 1;
            descriptorWrites[9].pImageInfo = &shadowmap_image_info;
            
            vk::DescriptorBufferInfo mesh_info;
            mesh_info.buffer = resources->mesh_info_buffer->buffer;
            mesh_info.offset = sizeof(GlobalResources::MeshInfo) * GlobalResources::max_resource_index * i;
            mesh_info.range = sizeof(GlobalResources::MeshInfo) * GlobalResources::max_resource_index;

            descriptorWrites[10].dstSet = *deferred_descriptor_set[i];
            descriptorWrites[10].dstBinding = 11;
            descriptorWrites[10].dstArrayElement = 0;
            descriptorWrites[10].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[10].descriptorCount = 1;
            descriptorWrites[10].pBufferInfo = &mesh_info;

            if (resources->descriptor_material_info.size() > 0)
            {
                descriptorWrites.push_back({});
                descriptorWrites[11].dstSet = *deferred_descriptor_set[i];
                descriptorWrites[11].dstBinding = 10;
                descriptorWrites[11].dstArrayElement = 0;
                descriptorWrites[11].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[11].descriptorCount = resources->descriptor_material_info.size();
                descriptorWrites[11].pBufferInfo = resources->descriptor_material_info.data();
            }

            gpu->device->updateDescriptorSets(descriptorWrites, {});
            buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *deferred_pipeline_layout, 0, *deferred_descriptor_set[i], {});

            buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *deferred_pipeline);

            buffer.draw(3, 1, 0, 0);

            buffer.endRenderPass();
            buffer.end();
        }
    }

    Task<> RendererRasterization::resizeRenderer()
    {
        co_await recreateRenderer();
        if (engine->camera)
        {
            engine->camera->setPerspective(glm::radians(70.f), engine->renderer->swapchain->extent.width / (float)engine->renderer->swapchain->extent.height, 0.01f, 1000.f);
        }
    }

    Task<> RendererRasterization::recreateRenderer()
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
        createAnimationResources();
        //recreate command buffers
        co_await recreateStaticCommandBuffers();
    }

    void RendererRasterization::initializeCameraBuffers()
    {
        camera_buffers.view_proj_ubo = engine->renderer->gpu->memory_manager->GetBuffer(engine->renderer->uniform_buffer_align_up(sizeof(Camera::CameraData)) * getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        camera_buffers.view_proj_mapped = static_cast<uint8_t*>(camera_buffers.view_proj_ubo->map(0, engine->renderer->uniform_buffer_align_up(sizeof(Camera::CameraData)) * getImageCount(), {}));

        camera_buffers.cascade_data_ubo = engine->renderer->gpu->memory_manager->GetBuffer(engine->renderer->uniform_buffer_align_up(sizeof(cascade_data)) * engine->renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        camera_buffers.cascade_data_mapped = static_cast<uint8_t*>(camera_buffers.cascade_data_ubo->map(0, engine->renderer->uniform_buffer_align_up(sizeof(cascade_data)) * engine->renderer->getImageCount(), {}));
    }

    vk::CommandBuffer RendererRasterization::getRenderCommandbuffer(uint32_t image_index)
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.commandPool = *command_pool;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = 1;

        auto buffer = gpu->device->allocateCommandBuffersUnique(alloc_info);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        buffer[0]->begin(begin_info);

        vk::RenderPassBeginInfo renderpass_info = {};
        renderpass_info.renderArea.offset = vk::Offset2D{ 0, 0 };

        renderpass_info.renderPass = *shadowmap_render_pass;
        renderpass_info.renderArea.extent = vk::Extent2D{ engine->settings.renderer_settings.shadowmap_dimension, engine->settings.renderer_settings.shadowmap_dimension };

        std::array<vk::ClearValue, 1> clearValue = {};
        clearValue[0].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

        renderpass_info.clearValueCount = static_cast<uint32_t>(clearValue.size());
        renderpass_info.pClearValues = clearValue.data();

        auto shadowmap_buffers = engine->worker_pool->getShadowmapGraphicsBuffers(image_index);

        for (uint32_t i = 0; i < shadowmap_cascades; ++i)
        {
            renderpass_info.framebuffer = *cascades[i].shadowmap_frame_buffer;
            buffer[0]->pushConstants<uint32_t>(*shadowmap_pipeline_layout, vk::ShaderStageFlagBits::eVertex, sizeof(uint32_t), i);
            buffer[0]->beginRenderPass(renderpass_info, vk::SubpassContents::eSecondaryCommandBuffers);
            if (!shadowmap_buffers.empty())
            {
                buffer[0]->executeCommands(shadowmap_buffers);
            }
            buffer[0]->endRenderPass();
        }

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

        buffer[0]->end();
        render_commandbuffers[image_index] = std::move(buffer[0]);
        return *render_commandbuffers[image_index];
    }

    Task<> RendererRasterization::drawFrame()
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

            updateCameraBuffers();
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

            std::vector<vk::Semaphore> gbuffer_semaphores = { *gbuffer_sem };
            submitInfo.pSignalSemaphores = gbuffer_semaphores.data();
            submitInfo.signalSemaphoreCount = gbuffer_semaphores.size();

            gpu->graphics_queue.submit(submitInfo, nullptr);

            submitInfo.waitSemaphoreCount = gbuffer_semaphores.size();
            submitInfo.pWaitSemaphores = gbuffer_semaphores.data();
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

    void RendererRasterization::initEntity(EntityInitializer* initializer, Engine* engine)
    {
        initializer->initEntity(this, engine);
    }

    void RendererRasterization::drawEntity(EntityInitializer* initializer, Engine* engine)
    {
        initializer->drawEntity(this, engine);
    }

    vk::Pipeline lotus::RendererRasterization::createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info)
    {
        info.layout = *pipeline_layout;
        info.renderPass = *gbuffer_render_pass;
        std::lock_guard lk{ shutdown_mutex };
        return *pipelines.emplace_back(gpu->device->createGraphicsPipelineUnique(nullptr, info, nullptr));
    }

    vk::Pipeline lotus::RendererRasterization::createShadowmapPipeline(vk::GraphicsPipelineCreateInfo& info)
    {
        info.layout = *shadowmap_pipeline_layout;
        info.renderPass = *shadowmap_render_pass;
        std::lock_guard lk{ shutdown_mutex };
        return *pipelines.emplace_back(gpu->device->createGraphicsPipelineUnique(nullptr, info, nullptr));
    }

    void RendererRasterization::bindResources(uint32_t image, vk::WriteDescriptorSet vertex, vk::WriteDescriptorSet index,
        vk::WriteDescriptorSet material, vk::WriteDescriptorSet texture, vk::WriteDescriptorSet mesh_info)
    {
    }
}
