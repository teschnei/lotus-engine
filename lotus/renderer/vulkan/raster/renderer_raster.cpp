module;

#include <algorithm>
#include <coroutine>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

module lotus;

import :renderer.vulkan.renderer.raster;

import :core.config;
import :core.engine;
import :core.game;
import :core.light_manager;
import :util;
import glm;
import vulkan_hpp;

namespace lotus
{
RendererRasterization::RendererRasterization(Engine* _engine) : Renderer(_engine) {}

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
    rasterizer = std::make_unique<RasterPipeline>(this);
    createGraphicsPipeline();
    createDepthImage();
    createSyncs();
    createCommandPool();
    createShadowmapResources();
    createAnimationResources();
    createDeferredImage();

    initializeCameraBuffers();
    generateCommandBuffers();

    co_return;
}

void RendererRasterization::generateCommandBuffers() {}

void RendererRasterization::updateCameraBuffers()
{
    engine->camera->writeToBuffer(
        *(Component::CameraComponent::CameraData*)(((uint8_t*)camera_buffers.view_proj_mapped) +
                                                   uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)) * current_frame));
    memcpy(camera_buffers.cascade_data_mapped + (getCurrentFrame() * uniform_buffer_align_up(sizeof(cascade_data))), &cascade_data, sizeof(cascade_data));
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
    dependency.srcSubpass = vk::SubpassExternal;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

    std::array<vk::AttachmentDescription, 2> attachments = {color_attachment, depth_attachment};
    vk::RenderPassCreateInfo render_pass_info;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    depth_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    depth_attachment_ref.attachment = 0;
    depth_attachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    std::array<vk::AttachmentDescription, 1> shadow_attachments = {depth_attachment};
    subpass.colorAttachmentCount = 0;
    subpass.pColorAttachments = nullptr;
    render_pass_info.attachmentCount = static_cast<uint32_t>(shadow_attachments.size());
    render_pass_info.pAttachments = shadow_attachments.data();

    vk::SubpassDependency shadowmap_dep1;
    shadowmap_dep1.srcSubpass = vk::SubpassExternal;
    shadowmap_dep1.dstSubpass = 0;
    shadowmap_dep1.srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    shadowmap_dep1.dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    shadowmap_dep1.srcAccessMask = vk::AccessFlagBits::eShaderRead;
    shadowmap_dep1.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    shadowmap_dep1.dependencyFlags = vk::DependencyFlagBits::eByRegion;

    vk::SubpassDependency shadowmap_dep2;
    shadowmap_dep2.srcSubpass = 0;
    shadowmap_dep2.dstSubpass = vk::SubpassExternal;
    shadowmap_dep2.srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests;
    shadowmap_dep2.dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    shadowmap_dep2.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    shadowmap_dep2.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    shadowmap_dep2.dependencyFlags = vk::DependencyFlagBits::eByRegion;

    std::array<vk::SubpassDependency, 2> shadowmap_subpass_deps = {shadowmap_dep1, shadowmap_dep2};

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

    vk::DescriptorSetLayoutBinding mesh_info_layout_binding;
    mesh_info_layout_binding.binding = 3;
    mesh_info_layout_binding.descriptorCount = 1;
    mesh_info_layout_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
    mesh_info_layout_binding.pImmutableSamplers = nullptr;
    mesh_info_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutBinding material_info_layout_binding;
    material_info_layout_binding.binding = 4;
    material_info_layout_binding.descriptorCount = 1;
    material_info_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    material_info_layout_binding.pImmutableSamplers = nullptr;
    material_info_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptor;

    vk::DescriptorSetLayoutBinding cascade_matrices;
    cascade_matrices.binding = 3;
    cascade_matrices.descriptorCount = 1;
    cascade_matrices.descriptorType = vk::DescriptorType::eUniformBuffer;
    cascade_matrices.pImmutableSamplers = nullptr;
    cascade_matrices.stageFlags = vk::ShaderStageFlagBits::eVertex;

    std::vector<vk::DescriptorSetLayoutBinding> shadowmap_bindings = {camera_layout_binding, model_layout_binding, cascade_matrices, sample_layout_binding};

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

    vk::DescriptorSetLayoutBinding light_type_sample_layout_binding;
    light_type_sample_layout_binding.binding = 3;
    light_type_sample_layout_binding.descriptorCount = 1;
    light_type_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    light_type_sample_layout_binding.pImmutableSamplers = nullptr;
    light_type_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

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
    light_deferred_layout_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
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

    std::vector<vk::DescriptorSetLayoutBinding> deferred_bindings = {
        pos_sample_layout_binding,           normal_sample_layout_binding,     albedo_sample_layout_binding,   light_type_sample_layout_binding,
        accumulation_sample_layout_binding,  revealage_sample_layout_binding,  camera_deferred_layout_binding, light_deferred_layout_binding,
        lightsource_deferred_layout_binding, shadowmap_deferred_layout_binding};

    layout_info.flags = {};
    layout_info.bindingCount = static_cast<uint32_t>(deferred_bindings.size());
    layout_info.pBindings = deferred_bindings.data();

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

    vk::DescriptorPoolCreateInfo pool_ci;
    pool_ci.maxSets = 3;
    pool_ci.poolSizeCount = static_cast<uint32_t>(pool_sizes_deferred.size());
    pool_ci.pPoolSizes = pool_sizes_deferred.data();
    pool_ci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

    deferred_descriptor_pool = gpu->device->createDescriptorPoolUnique(pool_ci, nullptr);

    std::array<vk::DescriptorSetLayout, 3> layouts = {*deferred_descriptor_set_layout, *deferred_descriptor_set_layout, *deferred_descriptor_set_layout};

    vk::DescriptorSetAllocateInfo set_ci;
    set_ci.descriptorPool = *deferred_descriptor_pool;
    set_ci.descriptorSetCount = 3;
    set_ci.pSetLayouts = layouts.data();
    deferred_descriptor_set = gpu->device->allocateDescriptorSetsUnique(set_ci);
}

void RendererRasterization::createGraphicsPipeline()
{
    vk::PipelineLayoutCreateInfo pipeline_layout_info;

    // material index
    vk::PushConstantRange push_constant_range;
    push_constant_range.stageFlags = vk::ShaderStageFlagBits::eFragment;
    push_constant_range.size = 4;
    push_constant_range.offset = 0;

    pipeline_layout_info.pPushConstantRanges = &push_constant_range;
    pipeline_layout_info.pushConstantRangeCount = 1;

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
    color_blend_attachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
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

    std::array<vk::DescriptorSetLayout, 1> deferred_descriptor_layouts = {*deferred_descriptor_set_layout};

    pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(deferred_descriptor_layouts.size());
    pipeline_layout_info.pSetLayouts = deferred_descriptor_layouts.data();

    deferred_pipeline_layout = gpu->device->createPipelineLayoutUnique(pipeline_layout_info, nullptr);

    std::array dynamic_states{vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamic_state{.dynamicStateCount = dynamic_states.size(), .pDynamicStates = dynamic_states.data()};

    std::array attachment_formats{vk::Format::eR32G32B32A32Sfloat};

    vk::PipelineRenderingCreateInfo rendering_info{.viewMask = 0,
                                                   .colorAttachmentCount = attachment_formats.size(),
                                                   .pColorAttachmentFormats = attachment_formats.data(),
                                                   .depthAttachmentFormat = gpu->getDepthFormat()};

    vk::GraphicsPipelineCreateInfo pipeline_info{.pNext = &rendering_info,
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
                                                 .basePipelineHandle = nullptr};

    deferred_pipeline = gpu->device->createGraphicsPipelineUnique(nullptr, pipeline_info, nullptr).value;

    std::array<vk::DescriptorSetLayout, 1> shadowmap_descriptor_layouts = {*shadowmap_descriptor_set_layout};

    vk::PushConstantRange push_constant_range_shadowmap;
    push_constant_range_shadowmap.stageFlags = vk::ShaderStageFlagBits::eVertex;
    push_constant_range_shadowmap.size = sizeof(uint32_t);
    push_constant_range_shadowmap.offset = 4;

    std::vector<vk::PushConstantRange> push_constant_ranges = {push_constant_range, push_constant_range_shadowmap};

    pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(shadowmap_descriptor_layouts.size());
    pipeline_layout_info.pSetLayouts = shadowmap_descriptor_layouts.data();
    pipeline_layout_info.pushConstantRangeCount = push_constant_ranges.size();
    pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();

    shadowmap_pipeline_layout = gpu->device->createPipelineLayoutUnique(pipeline_layout_info, nullptr);
}

void RendererRasterization::createDepthImage()
{
    auto format = gpu->getDepthFormat();

    depth_image = gpu->memory_manager->GetImage(swapchain->extent.width, swapchain->extent.height, format, vk::ImageTiling::eOptimal,
                                                vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);

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

void RendererRasterization::createSyncs()
{
    vk::SemaphoreTypeCreateInfo semaphore_type{.semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = timeline_frame_ready};
    for (auto i = 0; i < max_pending_frames; ++i)
    {
        frame_timeline_sem.push_back(gpu->device->createSemaphoreUnique({.pNext = &semaphore_type}));
        timeline_sem_base.push_back(0);
    }
}

void RendererRasterization::createShadowmapResources()
{
    auto format = gpu->getDepthFormat();

    shadowmap_image = gpu->memory_manager->GetImage(
        engine->settings.renderer_settings.shadowmap_dimension, engine->settings.renderer_settings.shadowmap_dimension, format, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, shadowmap_cascades);

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

        std::array<vk::ImageView, 1> attachments = {*cascade.shadowmap_image_view};

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

vk::UniqueCommandBuffer RendererRasterization::getDeferredCommandBuffer()
{
    vk::CommandBufferAllocateInfo alloc_info = {};
    alloc_info.commandPool = *command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = 1;

    auto deferred_command_buffers = gpu->device->allocateCommandBuffersUnique(alloc_info);

    vk::CommandBuffer buffer = *deferred_command_buffers[0];

    buffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    std::array pre_render_transitions{vk::ImageMemoryBarrier2{
        .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
        .srcAccessMask = {},
        .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = deferred_image->image,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}}};

    buffer.pipelineBarrier2(
        {.imageMemoryBarrierCount = static_cast<uint32_t>(pre_render_transitions.size()), .pImageMemoryBarriers = pre_render_transitions.data()});

    std::array colour_attachments{vk::RenderingAttachmentInfo{.imageView = *deferred_image_view,
                                                              .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                              .loadOp = vk::AttachmentLoadOp::eClear,
                                                              .storeOp = vk::AttachmentStoreOp::eStore,
                                                              .clearValue = {.color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}}};

    vk::RenderingAttachmentInfo depth_info{.imageView = *depth_image_view,
                                           .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                                           .loadOp = vk::AttachmentLoadOp::eClear,
                                           .storeOp = vk::AttachmentStoreOp::eDontCare,
                                           .clearValue = {.depthStencil = vk::ClearDepthStencilValue{1.0f, 0}}};

    buffer.beginRendering({.renderArea = {.extent = swapchain->extent},
                           .layerCount = 1,
                           .viewMask = 0,
                           .colorAttachmentCount = colour_attachments.size(),
                           .pColorAttachments = colour_attachments.data(),
                           .pDepthAttachment = &depth_info});

    vk::DescriptorImageInfo pos_info;
    pos_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    pos_info.imageView = *rasterizer->getGBuffer().position.image_view;
    pos_info.sampler = *rasterizer->getGBuffer().sampler;

    vk::DescriptorImageInfo normal_info;
    normal_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    normal_info.imageView = *rasterizer->getGBuffer().normal.image_view;
    normal_info.sampler = *rasterizer->getGBuffer().sampler;

    vk::DescriptorImageInfo albedo_info;
    albedo_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    albedo_info.imageView = *rasterizer->getGBuffer().albedo.image_view;
    albedo_info.sampler = *rasterizer->getGBuffer().sampler;

    vk::DescriptorImageInfo light_type_info;
    light_type_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    light_type_info.imageView = *rasterizer->getGBuffer().light_type.image_view;
    light_type_info.sampler = *rasterizer->getGBuffer().sampler;

    vk::DescriptorImageInfo accumulation_info;
    accumulation_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    accumulation_info.imageView = *rasterizer->getGBuffer().accumulation.image_view;
    accumulation_info.sampler = *rasterizer->getGBuffer().sampler;

    vk::DescriptorImageInfo revealage_info;
    revealage_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    revealage_info.imageView = *rasterizer->getGBuffer().revealage.image_view;
    revealage_info.sampler = *rasterizer->getGBuffer().sampler;

    vk::DescriptorBufferInfo camera_buffer_info;
    camera_buffer_info.buffer = camera_buffers.view_proj_ubo->buffer;
    camera_buffer_info.offset = current_frame * uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData));
    camera_buffer_info.range = sizeof(Component::CameraComponent::CameraData);

    std::vector<vk::WriteDescriptorSet> descriptorWrites{10};

    descriptorWrites[0].dstSet = *deferred_descriptor_set[current_frame];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &pos_info;

    descriptorWrites[1].dstSet = *deferred_descriptor_set[current_frame];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &normal_info;

    descriptorWrites[2].dstSet = *deferred_descriptor_set[current_frame];
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pImageInfo = &albedo_info;

    descriptorWrites[3].dstSet = *deferred_descriptor_set[current_frame];
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pImageInfo = &light_type_info;

    descriptorWrites[4].dstSet = *deferred_descriptor_set[current_frame];
    descriptorWrites[4].dstBinding = 4;
    descriptorWrites[4].dstArrayElement = 0;
    descriptorWrites[4].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    descriptorWrites[4].descriptorCount = 1;
    descriptorWrites[4].pImageInfo = &accumulation_info;

    descriptorWrites[5].dstSet = *deferred_descriptor_set[current_frame];
    descriptorWrites[5].dstBinding = 5;
    descriptorWrites[5].dstArrayElement = 0;
    descriptorWrites[5].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    descriptorWrites[5].descriptorCount = 1;
    descriptorWrites[5].pImageInfo = &revealage_info;

    descriptorWrites[6].dstSet = *deferred_descriptor_set[current_frame];
    descriptorWrites[6].dstBinding = 6;
    descriptorWrites[6].dstArrayElement = 0;
    descriptorWrites[6].descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrites[6].descriptorCount = 1;
    descriptorWrites[6].pBufferInfo = &camera_buffer_info;

    vk::DescriptorBufferInfo light_buffer_info;
    light_buffer_info.buffer = engine->lights->light_buffer->buffer;
    light_buffer_info.offset = current_frame * engine->lights->GetBufferSize();
    light_buffer_info.range = engine->lights->GetBufferSize();

    vk::DescriptorImageInfo shadowmap_image_info;
    shadowmap_image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    shadowmap_image_info.imageView = *shadowmap_image_view;
    shadowmap_image_info.sampler = *shadowmap_sampler;

    vk::DescriptorBufferInfo cascade_buffer_info;
    cascade_buffer_info.buffer = camera_buffers.cascade_data_ubo->buffer;
    cascade_buffer_info.offset = current_frame * uniform_buffer_align_up(sizeof(cascade_data));
    cascade_buffer_info.range = sizeof(cascade_data);

    descriptorWrites[7].dstSet = *deferred_descriptor_set[current_frame];
    descriptorWrites[7].dstBinding = 7;
    descriptorWrites[7].dstArrayElement = 0;
    descriptorWrites[7].descriptorType = vk::DescriptorType::eStorageBuffer;
    descriptorWrites[7].descriptorCount = 1;
    descriptorWrites[7].pBufferInfo = &light_buffer_info;

    descriptorWrites[8].dstSet = *deferred_descriptor_set[current_frame];
    descriptorWrites[8].dstBinding = 8;
    descriptorWrites[8].dstArrayElement = 0;
    descriptorWrites[8].descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrites[8].descriptorCount = 1;
    descriptorWrites[8].pBufferInfo = &cascade_buffer_info;

    descriptorWrites[9].dstSet = *deferred_descriptor_set[current_frame];
    descriptorWrites[9].dstBinding = 9;
    descriptorWrites[9].dstArrayElement = 0;
    descriptorWrites[9].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    descriptorWrites[9].descriptorCount = 1;
    descriptorWrites[9].pImageInfo = &shadowmap_image_info;

    gpu->device->updateDescriptorSets(descriptorWrites, {});
    buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *deferred_pipeline_layout, 0, *deferred_descriptor_set[current_frame], {});

    buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *deferred_pipeline);

    vk::Viewport viewport{.x = 0.0f,
                          .y = 0.0f,
                          .width = (float)engine->renderer->swapchain->extent.width,
                          .height = (float)engine->renderer->swapchain->extent.height,
                          .minDepth = 0.0f,
                          .maxDepth = 1.0f};

    vk::Rect2D scissor{.offset = vk::Offset2D{0, 0}, .extent = engine->renderer->swapchain->extent};

    buffer.setScissor(0, scissor);
    buffer.setViewport(0, viewport);

    buffer.draw(3, 1, 0, 0);

    buffer.endRendering();
    buffer.end();

    return std::move(deferred_command_buffers[0]);
}

Task<> RendererRasterization::recreateRenderer()
{
    gpu->device->waitIdle();
    engine->worker_pool->Reset();
    swapchain->recreateSwapchain(current_image);

    createRenderpasses();
    createDepthImage();
    // can skip this if scissor/viewport are dynamic
    createGraphicsPipeline();
    createAnimationResources();
    // recreate command buffers
    co_await recreateStaticCommandBuffers();
    co_await ui->ReInit();
}

void RendererRasterization::initializeCameraBuffers()
{
    camera_buffers.view_proj_ubo = engine->renderer->gpu->memory_manager->GetBuffer(
        engine->renderer->uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)) * getFrameCount(), vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    camera_buffers.view_proj_mapped = static_cast<Component::CameraComponent::CameraData*>(
        camera_buffers.view_proj_ubo->map(0, engine->renderer->uniform_buffer_align_up(sizeof(Component::CameraComponent::CameraData)) * getFrameCount(), {}));

    camera_buffers.cascade_data_ubo = engine->renderer->gpu->memory_manager->GetBuffer(
        engine->renderer->uniform_buffer_align_up(sizeof(cascade_data)) * engine->renderer->getFrameCount(), vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    camera_buffers.cascade_data_mapped = static_cast<uint8_t*>(
        camera_buffers.cascade_data_ubo->map(0, engine->renderer->uniform_buffer_align_up(sizeof(cascade_data)) * engine->renderer->getFrameCount(), {}));
}

std::vector<vk::CommandBuffer> RendererRasterization::getRenderCommandbuffers()
{
    auto secondary_buffers = engine->worker_pool->getSecondaryGraphicsBuffers(current_frame);
    auto transparent_buffers = engine->worker_pool->getParticleGraphicsBuffers(current_frame);
    std::vector<vk::CommandBuffer> render_buffers(secondary_buffers.size() + transparent_buffers.size() + 3);

    auto buffer = gpu->device->allocateCommandBuffersUnique({
        .commandPool = *command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 3,
    });

    buffer[0]->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // TODO: shadowmap renderpass first

    /*
    vk::RenderPassBeginInfo renderpass_info = {};
    renderpass_info.renderArea.offset = vk::Offset2D{ 0, 0 };

    renderpass_info.renderPass = *shadowmap_render_pass;
    renderpass_info.renderArea.extent = vk::Extent2D{ engine->settings.renderer_settings.shadowmap_dimension,
    engine->settings.renderer_settings.shadowmap_dimension };

    std::array<vk::ClearValue, 1> clearValue = {};
    clearValue[0].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

    renderpass_info.clearValueCount = static_cast<uint32_t>(clearValue.size());
    renderpass_info.pClearValues = clearValue.data();

    auto shadowmap_buffers = engine->worker_pool->getShadowmapGraphicsBuffers(current_frame);

    for (uint32_t i = 0; i < shadowmap_cascades; ++i)
    {
        renderpass_info.framebuffer = *cascades[i].shadowmap_frame_buffer;
        buffer[0]->pushConstants<uint32_t>(*shadowmap_pipeline_layout, vk::ShaderStageFlagBits::eVertex,
    sizeof(uint32_t), i); buffer[0]->beginRenderPass(renderpass_info, vk::SubpassContents::eSecondaryCommandBuffers); if
    (!shadowmap_buffers.empty())
        {
            buffer[0]->executeCommands(shadowmap_buffers);
        }
        buffer[0]->endRenderPass();
    }
    */

    rasterizer->beginRendering(*buffer[0]);
    rasterizer->beginMainCommandBufferRendering(*buffer[0], vk::RenderingFlagBits::eSuspending);
    buffer[0]->endRendering();
    buffer[0]->end();

    render_buffers[0] = *buffer[0];
    std::ranges::copy(secondary_buffers, render_buffers.begin() + 1);

    buffer[1]->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    rasterizer->beginMainCommandBufferRendering(*buffer[1], vk::RenderingFlagBits::eResuming);
    buffer[1]->endRendering();
    rasterizer->beginTransparencyCommandBufferRendering(*buffer[1], vk::RenderingFlagBits::eSuspending);
    buffer[1]->endRendering();
    buffer[1]->end();

    render_buffers[1 + secondary_buffers.size()] = *buffer[1];
    std::ranges::copy(transparent_buffers, render_buffers.begin() + 2 + secondary_buffers.size());

    buffer[2]->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    rasterizer->beginTransparencyCommandBufferRendering(*buffer[2], vk::RenderingFlagBits::eResuming);
    buffer[2]->endRendering();
    rasterizer->endRendering(*buffer[2]);
    buffer[2]->end();

    render_buffers.back() = *buffer[2];

    engine->worker_pool->gpuResource(std::move(buffer));

    return render_buffers;
}

Task<> RendererRasterization::drawFrame()
{
    if (!engine->game || !engine->game->scene)
        co_return;

    global_descriptors->updateDescriptorSet();

    engine->worker_pool->deleteFinished();
    uint64_t frame_ready_value = timeline_sem_base[current_frame] + timeline_frame_ready;
    gpu->device->waitSemaphores({.semaphoreCount = 1, .pSemaphores = &*frame_timeline_sem[current_frame], .pValues = &frame_ready_value},
                                std::numeric_limits<uint64_t>::max());
    timeline_sem_base[current_frame] = timeline_sem_base[current_frame] + timeline_frame_ready;

    try
    {
        previous_image = current_image;
        current_image =
            gpu->device->acquireNextImageKHR(*swapchain->swapchain, std::numeric_limits<uint64_t>::max(), *image_ready_sem[current_frame], nullptr).value;
    }
    catch (vk::OutOfDateKHRError&)
    {
    }

    try
    {
        engine->worker_pool->clearProcessed(current_frame);
        swapchain->checkOldSwapchain(current_frame);

        engine->worker_pool->beginProcessing(current_frame);

        updateCameraBuffers();
        engine->lights->UpdateLightBuffer();

        auto buffers_temp = engine->worker_pool->getPrimaryGraphicsBuffers(current_frame);
        auto render_buffers = getRenderCommandbuffers();
        std::vector<vk::CommandBufferSubmitInfoKHR> buffers;
        buffers.resize(buffers_temp.size() + render_buffers.size());
        std::ranges::transform(buffers_temp, buffers.begin(), [](auto buffer) { return vk::CommandBufferSubmitInfoKHR{.commandBuffer = buffer}; });
        std::ranges::transform(render_buffers, buffers.begin() + buffers_temp.size(),
                               [](auto buffer) { return vk::CommandBufferSubmitInfoKHR{.commandBuffer = buffer}; });

        vk::SemaphoreSubmitInfoKHR graphics_sem{.semaphore = *frame_timeline_sem[current_frame],
                                                .value = timeline_sem_base[current_frame] + timeline_graphics,
                                                .stageMask = vk::PipelineStageFlagBits2::eAllCommands};

        auto deferred_buffer = getDeferredCommandBuffer();
        auto ui_buffers = ui->Render();
        std::vector<vk::CommandBufferSubmitInfoKHR> deferred_buffers{{.commandBuffer = *deferred_buffer}};
        deferred_buffers.resize(1 + ui_buffers.size());
        std::ranges::transform(ui_buffers, deferred_buffers.begin() + 1, [](auto buffer) { return vk::CommandBufferSubmitInfoKHR{.commandBuffer = buffer}; });
        deferred_buffers.push_back({.commandBuffer = prepareDeferredImageForPresent()});

        std::array deferred_waits{graphics_sem, vk::SemaphoreSubmitInfoKHR{.semaphore = *image_ready_sem[current_frame],
                                                                           .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput}};

        std::array deferred_signals{
            vk::SemaphoreSubmitInfoKHR{.semaphore = *frame_timeline_sem[current_frame],
                                       .value = timeline_sem_base[current_frame] + timeline_frame_ready,
                                       .stageMask = vk::PipelineStageFlagBits2::eAllCommands},
            vk::SemaphoreSubmitInfoKHR{.semaphore = *frame_finish_sem[current_image], .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput}};

        gpu->graphics_queue.submit2({vk::SubmitInfo2{.commandBufferInfoCount = static_cast<uint32_t>(buffers.size()),
                                                     .pCommandBufferInfos = buffers.data(),
                                                     .signalSemaphoreInfoCount = 1,
                                                     .pSignalSemaphoreInfos = &graphics_sem},
                                     vk::SubmitInfo2{.waitSemaphoreInfoCount = deferred_waits.size(),
                                                     .pWaitSemaphoreInfos = deferred_waits.data(),
                                                     .commandBufferInfoCount = static_cast<uint32_t>(deferred_buffers.size()),
                                                     .pCommandBufferInfos = deferred_buffers.data(),
                                                     .signalSemaphoreInfoCount = deferred_signals.size(),
                                                     .pSignalSemaphoreInfos = deferred_signals.data()}});

        engine->worker_pool->gpuResource(std::move(deferred_buffer));

        std::vector<vk::Semaphore> present_waits{*frame_finish_sem[current_image]};
        std::vector<vk::SwapchainKHR> swap_chains = {*swapchain->swapchain};

        co_await engine->worker_pool->mainThread();

        gpu->present_queue.presentKHR({.waitSemaphoreCount = static_cast<uint32_t>(present_waits.size()),
                                       .pWaitSemaphores = present_waits.data(),
                                       .swapchainCount = static_cast<uint32_t>(swap_chains.size()),
                                       .pSwapchains = swap_chains.data(),
                                       .pImageIndices = &current_image});

        previous_frame = current_frame;
        current_frame = (current_frame + 1) % max_pending_frames;
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

vk::Pipeline lotus::RendererRasterization::createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info)
{
    info.layout = rasterizer->getPipelineLayout();
    auto pipeline_rendering_info = rasterizer->getMainRenderPassInfo();
    info.pNext = &pipeline_rendering_info;
    std::lock_guard lk{shutdown_mutex};
    return *pipelines.emplace_back(gpu->device->createGraphicsPipelineUnique(nullptr, info, nullptr).value);
}

vk::Pipeline lotus::RendererRasterization::createParticlePipeline(vk::GraphicsPipelineCreateInfo& info)
{
    info.layout = rasterizer->getPipelineLayout();
    auto pipeline_rendering_info = rasterizer->getTransparentRenderPassInfo();
    info.pNext = &pipeline_rendering_info;
    std::lock_guard lk{shutdown_mutex};
    return *pipelines.emplace_back(gpu->device->createGraphicsPipelineUnique(nullptr, info, nullptr).value);
}

vk::Pipeline lotus::RendererRasterization::createShadowmapPipeline(vk::GraphicsPipelineCreateInfo& info)
{
    info.layout = *shadowmap_pipeline_layout;
    info.pNext = nullptr;
    // TODO
    // info.renderPass = *shadowmap_render_pass;
    std::lock_guard lk{shutdown_mutex};
    return *pipelines.emplace_back(gpu->device->createGraphicsPipelineUnique(nullptr, info, nullptr).value);
}
} // namespace lotus
