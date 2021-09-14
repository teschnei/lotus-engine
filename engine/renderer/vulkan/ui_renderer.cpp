#include "ui_renderer.h"

#include "renderer.h"
#include "engine/core.h"

namespace lotus
{
    UiRenderer::UiRenderer(Engine* _engine, Renderer* _renderer) : engine(_engine), renderer(_renderer)
    {
        createDescriptorSetLayout();
        createRenderpass();
        createDepthImage();
        createFrameBuffers();
        createPipeline();
        command_buffers.resize(renderer->getImageCount());
    }

    Task<> UiRenderer::Init()
    {
        co_await createBuffers();
    }

    Task<> UiRenderer::ReInit()
    {
        createDescriptorSetLayout();
        createRenderpass();
        createDepthImage();
        createFrameBuffers();
        createPipeline();
        command_buffers.clear();
        command_buffers.resize(renderer->getImageCount());
        co_await engine->ui->ReInit();
    }

    vk::CommandBuffer UiRenderer::Render(int image_index)
    {
        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *renderer->graphics_pool;
        alloc_info.commandBufferCount = 1;

        auto buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

        vk::CommandBufferBeginInfo begin_info;
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        buffers[0]->begin(begin_info);

        std::array clear_values
        {
            vk::ClearValue{ .color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f } },
            vk::ClearValue{ .depthStencil = 1.f }
        };

        vk::RenderPassBeginInfo renderpass_begin;
        renderpass_begin.renderPass = *renderpass;
        renderpass_begin.renderArea.extent = renderer->swapchain->extent;
        renderpass_begin.clearValueCount = static_cast<uint32_t>(clear_values.size());
        renderpass_begin.pClearValues = clear_values.data();
        renderpass_begin.framebuffer = *framebuffers[image_index];

        buffers[0]->beginRenderPass(renderpass_begin, vk::SubpassContents::eSecondaryCommandBuffers);

        auto ui_buffers = engine->ui->getRenderCommandBuffers(image_index);
        if (!ui_buffers.empty())
        {
            buffers[0]->executeCommands(ui_buffers);
        }

        buffers[0]->endRenderPass();
        buffers[0]->end();

        command_buffers[image_index] = std::move(buffers[0]);
        return *command_buffers[image_index];
    }

    void UiRenderer::GenerateRenderBuffers(ui::Element* element)
    {
        for(size_t i = 0; i < element->command_buffers.size(); ++i)
        {
            auto& command_buffer = element->command_buffers[i];

            vk::CommandBufferInheritanceInfo inherit_info = {};
            inherit_info.renderPass = *renderpass;
            inherit_info.framebuffer = *framebuffers[i];

            vk::CommandBufferBeginInfo buffer_begin;
            buffer_begin.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue;
            buffer_begin.pInheritanceInfo = &inherit_info;

            command_buffer->begin(buffer_begin);

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

            command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *pipeline_layout, 0, {buffer_write, texture_write});

            command_buffer->bindVertexBuffers(0, {quad.vertex_buffer->buffer, quad.vertex_buffer->buffer}, {0, sizeof(glm::vec2)});

            command_buffer->draw(4, 1, 0, 0);

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
        layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();

        descriptor_set_layout = renderer->gpu->device->createDescriptorSetLayoutUnique(layout_info);
    }

    void UiRenderer::createPipeline()
    {
        auto vertex_module = renderer->getShader("shaders/ui_vert.spv");
        auto fragment_module = renderer->getShader("shaders/ui_frag.spv");

        vk::PipelineShaderStageCreateInfo vert_shader_stage_info;
        vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
        vert_shader_stage_info.module = *vertex_module;
        vert_shader_stage_info.pName = "main";

        vk::PipelineShaderStageCreateInfo frag_shader_stage_info;
        frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
        frag_shader_stage_info.module = *fragment_module;
        frag_shader_stage_info.pName = "main";

        std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages = {vert_shader_stage_info, frag_shader_stage_info};

        vk::PipelineVertexInputStateCreateInfo vertex_input_info;

        std::vector<vk::VertexInputBindingDescription> bindings {{0, sizeof(glm::vec4), vk::VertexInputRate::eVertex}, {1, sizeof(glm::vec4), vk::VertexInputRate::eVertex}};
        std::vector<vk::VertexInputAttributeDescription> attributes {{0, 0, vk::Format::eR32G32Sfloat, 0}, {1, 1, vk::Format::eR32G32Sfloat, 0}};

        vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vertex_input_info.pVertexBindingDescriptions = bindings.data();
        vertex_input_info.pVertexAttributeDescriptions = attributes.data();

        vk::PipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.topology = vk::PrimitiveTopology::eTriangleStrip;
        input_assembly.primitiveRestartEnable = false;

        vk::Viewport viewport;
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)renderer->swapchain->extent.width;
        viewport.height = (float)renderer->swapchain->extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vk::Rect2D scissor;
        scissor.offset = vk::Offset2D{0, 0};
        scissor.extent = renderer->swapchain->extent;

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
        std::array<vk::DescriptorSetLayout, 1> descriptor_layouts = { *descriptor_set_layout };

        vk::PipelineLayoutCreateInfo pipeline_layout_info;
        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_layouts.size());
        pipeline_layout_info.pSetLayouts = descriptor_layouts.data();
        pipeline_layout_info.pPushConstantRanges = &push_constant_range;
        pipeline_layout_info.pushConstantRangeCount = 1;

        pipeline_layout = renderer->gpu->device->createPipelineLayoutUnique(pipeline_layout_info);

        vk::GraphicsPipelineCreateInfo pipeline_info;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipeline_info.pStages = shader_stages.data();
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.layout = *pipeline_layout;
        pipeline_info.renderPass = *renderpass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = nullptr;

        pipeline = renderer->gpu->device->createGraphicsPipelineUnique(nullptr, pipeline_info);
    }

    void UiRenderer::createRenderpass()
    {
        vk::AttachmentDescription colour_attachment;
        colour_attachment.format = renderer->swapchain->image_format;
        colour_attachment.samples = vk::SampleCountFlagBits::e1;
        colour_attachment.loadOp = vk::AttachmentLoadOp::eLoad;
        colour_attachment.storeOp = vk::AttachmentStoreOp::eStore;
        colour_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        colour_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        colour_attachment.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
        colour_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentDescription depth_attachment;
        depth_attachment.format = renderer->gpu->getDepthFormat();
        depth_attachment.samples = vk::SampleCountFlagBits::e1;
        depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
        depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
        depth_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        depth_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        depth_attachment.initialLayout = vk::ImageLayout::eUndefined;
        depth_attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        std::vector<vk::AttachmentDescription> attachments {colour_attachment, depth_attachment};

        vk::AttachmentReference colour_reference {0, vk::ImageLayout::eColorAttachmentOptimal};
        vk::AttachmentReference depth_reference {1, vk::ImageLayout::eDepthStencilAttachmentOptimal};

        vk::SubpassDependency pre_subpass_dep;
        pre_subpass_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        pre_subpass_dep.dstSubpass = 0;
        pre_subpass_dep.srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
        pre_subpass_dep.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        pre_subpass_dep.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
        pre_subpass_dep.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        pre_subpass_dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;

        vk::SubpassDependency post_subpass_dep;
        post_subpass_dep.srcSubpass = 0;
        post_subpass_dep.dstSubpass = VK_SUBPASS_EXTERNAL;
        post_subpass_dep.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        post_subpass_dep.dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
        post_subpass_dep.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        post_subpass_dep.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
        post_subpass_dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;

        std::vector<vk::SubpassDependency> dependencies {pre_subpass_dep, post_subpass_dep};

        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colour_reference;
        subpass.pDepthStencilAttachment = &depth_reference;

        vk::RenderPassCreateInfo renderpass_ci;
        renderpass_ci.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderpass_ci.pAttachments = attachments.data();
        renderpass_ci.subpassCount = 1;
        renderpass_ci.pSubpasses = &subpass;
        renderpass_ci.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderpass_ci.pDependencies = dependencies.data();

        renderpass = renderer->gpu->device->createRenderPassUnique(renderpass_ci);
    }

    void UiRenderer::createDepthImage()
    {
        auto format = renderer->gpu->getDepthFormat();

        depth_image = renderer->gpu->memory_manager->GetImage(renderer->swapchain->extent.width, renderer->swapchain->extent.height, format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);

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

    void UiRenderer::createFrameBuffers()
    {
        framebuffers.clear();
        for (auto& swapchain_image_view : renderer->swapchain->image_views) {
            std::vector<vk::ImageView> attachments = {
                *swapchain_image_view,
                *depth_image_view
            };

            vk::FramebufferCreateInfo framebuffer_info = {};
            framebuffer_info.renderPass = *renderpass;
            framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebuffer_info.pAttachments = attachments.data();
            framebuffer_info.width = renderer->swapchain->extent.width;
            framebuffer_info.height = renderer->swapchain->extent.height;
            framebuffer_info.layers = 1;

            framebuffers.push_back(renderer->gpu->device->createFramebufferUnique(framebuffer_info, nullptr));
        }
    }
    Task<> UiRenderer::createBuffers()
    {
        std::vector<glm::vec4> vertex_buffer {
            {1.f, 1.f, 1.f, 1.f},
            {-1.f, 1.f, 0.f, 1.f},
            {1.f, -1.f, 1.f, 0.f},
            {-1.f, -1.f, 0.f, 0.f}
        };
        //TODO: these buffers should be device local since they never change
        quad.vertex_buffer = renderer->gpu->memory_manager->GetBuffer(vertex_buffer.size() * sizeof(glm::vec4), vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        void* buf_mem = quad.vertex_buffer->map(0, vertex_buffer.size() * sizeof(glm::vec4), {});
        memcpy(buf_mem, vertex_buffer.data(), vertex_buffer.size() * sizeof(glm::vec4));
        quad.vertex_buffer->unmap();

        default_texture = co_await Texture::LoadTexture("ui_default", [this](std::shared_ptr<Texture> texture) -> lotus::Task<>
        {
            VkDeviceSize imageSize = sizeof(glm::vec4);

            texture->setWidth(1);
            texture->setHeight(1);

            glm::vec4 data{1.f, 1.f, 1.f, 0.f};
            std::vector<uint8_t> texture_data;
            texture_data.resize(imageSize);
            memcpy(texture_data.data(), &data, imageSize);

            texture->image = renderer->gpu->memory_manager->GetImage(texture->getWidth(), texture->getHeight(), vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);

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

            std::vector<std::vector<uint8_t>> texture_datas{ std::move(texture_data) };
            co_await texture->Init(engine, std::move(texture_datas));
        });
    }
}
