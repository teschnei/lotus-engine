module;

#include <array>
#include <coroutine>
#include <cstring>
#include <memory>
#include <vector>

module lotus;

import :renderer.vulkan.ui_renderer;

import :core.engine;
import :ui;
import :renderer.vulkan.renderer;
import glm;
import vulkan_hpp;

namespace lotus
{
UiRenderer::UiRenderer(Engine* _engine, Renderer* _renderer) : engine(_engine), renderer(_renderer)
{
    createDescriptorSetLayout();
    createDepthImage();
    createPipeline();
}

Task<> UiRenderer::Init() { co_await createBuffers(); }

Task<> UiRenderer::ReInit()
{
    createDescriptorSetLayout();
    createDepthImage();
    createPipeline();
    co_await engine->ui->ReInit();
}

std::vector<vk::CommandBuffer> UiRenderer::Render()
{
    std::vector<vk::CommandBuffer> render_buffers;
    vk::CommandBufferAllocateInfo alloc_info{.commandPool = *renderer->graphics_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 2};

    auto buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

    buffers[0]->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    auto image = renderer->getCurrentImage();

    std::array colour_attachments{vk::RenderingAttachmentInfo{.imageView = *renderer->deferred_image_view,
                                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                                 .loadOp = vk::AttachmentLoadOp::eLoad,
                                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                                 .clearValue = {.color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}}};

    vk::RenderingAttachmentInfo depth_info{.imageView = *depth_image_view,
                                              .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                                              .loadOp = vk::AttachmentLoadOp::eClear,
                                              .storeOp = vk::AttachmentStoreOp::eDontCare,
                                              .clearValue = {.depthStencil = vk::ClearDepthStencilValue{1.0f, 0}}};

    buffers[0]->beginRendering({.flags = vk::RenderingFlagBits::eSuspending,
                                   .renderArea = {.extent = renderer->swapchain->extent},
                                   .layerCount = 1,
                                   .viewMask = 0,
                                   .colorAttachmentCount = colour_attachments.size(),
                                   .pColorAttachments = colour_attachments.data(),
                                   .pDepthAttachment = &depth_info});
    buffers[0]->endRendering();
    buffers[0]->end();

    auto ui_buffers = engine->ui->getRenderCommandBuffers(image);
    render_buffers.resize(2 + ui_buffers.size());
    render_buffers[0] = *buffers[0];
    std::ranges::copy(ui_buffers, render_buffers.begin() + 1);

    buffers[1]->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    buffers[1]->beginRendering({.flags = vk::RenderingFlagBits::eResuming,
                                   .renderArea = {.extent = renderer->swapchain->extent},
                                   .layerCount = 1,
                                   .viewMask = 0,
                                   .colorAttachmentCount = colour_attachments.size(),
                                   .pColorAttachments = colour_attachments.data(),
                                   .pDepthAttachment = &depth_info});
    buffers[1]->endRendering();

    buffers[1]->end();

    render_buffers.back() = *buffers[1];

    engine->worker_pool->gpuResource(std::move(buffers));
    return render_buffers;
}

void UiRenderer::GenerateRenderBuffers(ui::Element* element)
{
    for (size_t i = 0; i < element->command_buffers.size(); ++i)
    {
        auto& command_buffer = element->command_buffers[i];

        command_buffer->begin({.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse});

        std::array colour_attachments{vk::RenderingAttachmentInfo{.imageView = *renderer->deferred_image_view,
                                                                     .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                                     .loadOp = vk::AttachmentLoadOp::eLoad,
                                                                     .storeOp = vk::AttachmentStoreOp::eStore,
                                                                     .clearValue = {.color = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}}}};

        vk::RenderingAttachmentInfo depth_info{.imageView = *depth_image_view,
                                                  .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                                                  .loadOp = vk::AttachmentLoadOp::eClear,
                                                  .storeOp = vk::AttachmentStoreOp::eDontCare,
                                                  .clearValue = {.depthStencil = vk::ClearDepthStencilValue{1.0f, 0}}};

        command_buffer->beginRendering({.flags = vk::RenderingFlagBits::eResuming | vk::RenderingFlagBits::eSuspending,
                                           .renderArea = {.extent = renderer->swapchain->extent},
                                           .layerCount = 1,
                                           .viewMask = 0,
                                           .colorAttachmentCount = colour_attachments.size(),
                                           .pColorAttachments = colour_attachments.data(),
                                           .pDepthAttachment = &depth_info});

        command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

        command_buffer->pushConstants<vk::Extent2D>(*pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, {renderer->swapchain->extent});

        vk::DescriptorBufferInfo buffer_info;
        buffer_info.buffer = element->buffer->buffer;
        buffer_info.offset = i * renderer->uniform_buffer_align_up(sizeof(ui::Element::UniformBuffer));
        buffer_info.range = renderer->uniform_buffer_align_up(sizeof(ui::Element::UniformBuffer));

        vk::WriteDescriptorSet buffer_write;
        buffer_write.dstSet = nullptr;
        buffer_write.dstBinding = 0;
        buffer_write.dstArrayElement = 0;
        buffer_write.descriptorType = vk::DescriptorType::eUniformBuffer;
        buffer_write.descriptorCount = 1;
        buffer_write.pBufferInfo = &buffer_info;

        vk::DescriptorImageInfo texture_info;
        if (element->texture)
        {
            texture_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            texture_info.imageView = *element->texture->image_view;
            texture_info.sampler = *element->texture->sampler;
        }
        else
        {
            texture_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            texture_info.imageView = *default_texture->image_view;
            texture_info.sampler = *default_texture->sampler;
        }

        vk::WriteDescriptorSet texture_write;
        texture_write.dstSet = nullptr;
        texture_write.dstBinding = 1;
        texture_write.dstArrayElement = 0;
        texture_write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        texture_write.descriptorCount = 1;
        texture_write.pImageInfo = &texture_info;

        command_buffer->pushDescriptorSet(vk::PipelineBindPoint::eGraphics, *pipeline_layout, 0, {buffer_write, texture_write});

        vk::Viewport viewport{.x = 0.0f,
                              .y = 0.0f,
                              .width = (float)engine->renderer->swapchain->extent.width,
                              .height = (float)engine->renderer->swapchain->extent.height,
                              .minDepth = 0.0f,
                              .maxDepth = 1.0f};

        vk::Rect2D scissor{.offset = vk::Offset2D{0, 0}, .extent = engine->renderer->swapchain->extent};

        command_buffer->setScissor(0, scissor);
        command_buffer->setViewport(0, viewport);

        command_buffer->bindVertexBuffers(0, {quad.vertex_buffer->buffer, quad.vertex_buffer->buffer}, {0, sizeof(glm::vec2)});

        command_buffer->draw(4, 1, 0, 0);

        command_buffer->endRendering();

        command_buffer->end();
    }
}

void UiRenderer::createDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding buffer_binding;
    buffer_binding.binding = 0;
    buffer_binding.descriptorCount = 1;
    buffer_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    buffer_binding.pImmutableSamplers = nullptr;
    buffer_binding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutBinding texture_binding;
    texture_binding.binding = 1;
    texture_binding.descriptorCount = 1;
    texture_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    texture_binding.pImmutableSamplers = nullptr;
    texture_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    std::vector<vk::DescriptorSetLayoutBinding> bindings = {buffer_binding, texture_binding};

    vk::DescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptor;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    descriptor_set_layout = renderer->gpu->device->createDescriptorSetLayoutUnique(layout_info);
}

void UiRenderer::createPipeline()
{
    auto shader_module = renderer->getShader("shaders/ui.spv");

    vk::PipelineShaderStageCreateInfo vert_shader_stage_info;
    vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
    vert_shader_stage_info.module = *shader_module;
    vert_shader_stage_info.pName = "Vertex";

    vk::PipelineShaderStageCreateInfo frag_shader_stage_info;
    frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
    frag_shader_stage_info.module = *shader_module;
    frag_shader_stage_info.pName = "Fragment";

    std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages = {vert_shader_stage_info, frag_shader_stage_info};

    vk::PipelineVertexInputStateCreateInfo vertex_input_info;

    std::vector<vk::VertexInputBindingDescription> bindings{{0, sizeof(glm::vec4), vk::VertexInputRate::eVertex},
                                                            {1, sizeof(glm::vec4), vk::VertexInputRate::eVertex}};
    std::vector<vk::VertexInputAttributeDescription> attributes{{0, 0, vk::Format::eR32G32Sfloat, 0}, {1, 1, vk::Format::eR32G32Sfloat, 0}};

    vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertex_input_info.pVertexBindingDescriptions = bindings.data();
    vertex_input_info.pVertexAttributeDescriptions = attributes.data();

    vk::PipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.topology = vk::PrimitiveTopology::eTriangleStrip;
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
    color_blend_attachment.blendEnable = true;
    color_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;
    color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
    color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;

    std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachment_states{color_blend_attachment};

    vk::PipelineColorBlendStateCreateInfo color_blending;
    color_blending.logicOpEnable = false;
    color_blending.logicOp = vk::LogicOp::eCopy;
    color_blending.attachmentCount = static_cast<uint32_t>(color_blend_attachment_states.size());
    color_blending.pAttachments = color_blend_attachment_states.data();
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    vk::PushConstantRange push_constant_range;
    push_constant_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
    push_constant_range.size = sizeof(vk::Extent2D);
    push_constant_range.offset = 0;
    std::array<vk::DescriptorSetLayout, 1> descriptor_layouts = {*descriptor_set_layout};

    vk::PipelineLayoutCreateInfo pipeline_layout_info;
    pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_layouts.size());
    pipeline_layout_info.pSetLayouts = descriptor_layouts.data();
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;
    pipeline_layout_info.pushConstantRangeCount = 1;

    pipeline_layout = renderer->gpu->device->createPipelineLayoutUnique(pipeline_layout_info);

    std::array dynamic_states{vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamic_state{.dynamicStateCount = dynamic_states.size(), .pDynamicStates = dynamic_states.data()};

    std::array attachment_formats{vk::Format::eR32G32B32A32Sfloat};

    vk::PipelineRenderingCreateInfo rendering_info{.viewMask = 0,
                                                      .colorAttachmentCount = attachment_formats.size(),
                                                      .pColorAttachmentFormats = attachment_formats.data(),
                                                      .depthAttachmentFormat = renderer->gpu->getDepthFormat()};

    vk::GraphicsPipelineCreateInfo pipeline_info{.pNext = &rendering_info,
                                                 .stageCount = static_cast<uint32_t>(shader_stages.size()),
                                                 .pStages = shader_stages.data(),
                                                 .pVertexInputState = &vertex_input_info,
                                                 .pInputAssemblyState = &input_assembly,
                                                 .pViewportState = &viewport_state,
                                                 .pRasterizationState = &rasterizer,
                                                 .pMultisampleState = &multisampling,
                                                 .pDepthStencilState = &depth_stencil,
                                                 .pColorBlendState = &color_blending,
                                                 .pDynamicState = &dynamic_state,
                                                 .layout = *pipeline_layout,
                                                 .subpass = 0,
                                                 .basePipelineHandle = nullptr};

    pipeline = renderer->gpu->device->createGraphicsPipelineUnique(nullptr, pipeline_info).value;
}

void UiRenderer::createDepthImage()
{
    auto format = renderer->gpu->getDepthFormat();

    depth_image =
        renderer->gpu->memory_manager->GetImage(renderer->swapchain->extent.width, renderer->swapchain->extent.height, format, vk::ImageTiling::eOptimal,
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

    depth_image_view = renderer->gpu->device->createImageViewUnique(image_view_info, nullptr);
}

Task<> UiRenderer::createBuffers()
{
    std::vector<glm::vec4> vertex_buffer{{1.f, 1.f, 1.f, 1.f}, {-1.f, 1.f, 0.f, 1.f}, {1.f, -1.f, 1.f, 0.f}, {-1.f, -1.f, 0.f, 0.f}};
    // TODO: these buffers should be device local since they never change
    quad.vertex_buffer = renderer->gpu->memory_manager->GetBuffer(vertex_buffer.size() * sizeof(glm::vec4), vk::BufferUsageFlagBits::eVertexBuffer,
                                                                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    void* buf_mem = quad.vertex_buffer->map(0, vertex_buffer.size() * sizeof(glm::vec4), {});
    memcpy(buf_mem, vertex_buffer.data(), vertex_buffer.size() * sizeof(glm::vec4));
    quad.vertex_buffer->unmap();

    default_texture = co_await Texture::LoadTexture("ui_default",
                                                    [this](std::shared_ptr<Texture> texture) -> lotus::Task<>
                                                    {
                                                        vk::DeviceSize imageSize = sizeof(glm::vec4);

                                                        texture->setWidth(1);
                                                        texture->setHeight(1);

                                                        glm::vec4 data{1.f, 1.f, 1.f, 0.f};
                                                        std::vector<uint8_t> texture_data;
                                                        texture_data.resize(imageSize);
                                                        memcpy(texture_data.data(), &data, imageSize);

                                                        texture->image = renderer->gpu->memory_manager->GetImage(
                                                            texture->getWidth(), texture->getHeight(), vk::Format::eR32G32B32A32Sfloat,
                                                            vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                                                            vk::MemoryPropertyFlagBits::eDeviceLocal);

                                                        vk::ImageViewCreateInfo image_view_info;
                                                        image_view_info.image = texture->image->image;
                                                        image_view_info.viewType = vk::ImageViewType::e2D;
                                                        image_view_info.format = vk::Format::eR32G32B32A32Sfloat;
                                                        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
                                                        image_view_info.subresourceRange.baseMipLevel = 0;
                                                        image_view_info.subresourceRange.levelCount = 1;
                                                        image_view_info.subresourceRange.baseArrayLayer = 0;
                                                        image_view_info.subresourceRange.layerCount = 1;

                                                        texture->image_view = renderer->gpu->device->createImageViewUnique(image_view_info, nullptr);

                                                        vk::SamplerCreateInfo sampler_info = {};
                                                        sampler_info.magFilter = vk::Filter::eLinear;
                                                        sampler_info.minFilter = vk::Filter::eLinear;
                                                        sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
                                                        sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
                                                        sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
                                                        sampler_info.anisotropyEnable = true;
                                                        sampler_info.maxAnisotropy = 16;
                                                        sampler_info.borderColor = vk::BorderColor::eIntOpaqueBlack;
                                                        sampler_info.unnormalizedCoordinates = false;
                                                        sampler_info.compareEnable = false;
                                                        sampler_info.compareOp = vk::CompareOp::eAlways;
                                                        sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;

                                                        texture->sampler = renderer->gpu->device->createSamplerUnique(sampler_info, nullptr);

                                                        std::vector<std::vector<uint8_t>> texture_datas{std::move(texture_data)};
                                                        co_await texture->Init(engine, std::move(texture_datas));
                                                    });
}
} // namespace lotus
