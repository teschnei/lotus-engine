#include "d3m.h"

#include "engine/core.h"

namespace FFXI
{
    struct DatVertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        uint32_t color;
        glm::vec2 uv;
    };

    inline std::vector<vk::VertexInputBindingDescription> getBindingDescriptions()
    {
        std::vector<vk::VertexInputBindingDescription> binding_descriptions(1);

        binding_descriptions[0].binding = 0;
        binding_descriptions[0].stride = sizeof(D3M::Vertex);
        binding_descriptions[0].inputRate = vk::VertexInputRate::eVertex;

        return binding_descriptions;
    }

    inline std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions()
    {
        std::vector<vk::VertexInputAttributeDescription> attribute_descriptions(4);

        attribute_descriptions[0].binding = 0;
        attribute_descriptions[0].location = 0;
        attribute_descriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[0].offset = offsetof(D3M::Vertex, pos);

        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[1].offset = offsetof(D3M::Vertex, normal);

        attribute_descriptions[2].binding = 0;
        attribute_descriptions[2].location = 2;
        attribute_descriptions[2].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[2].offset = offsetof(D3M::Vertex, color);

        attribute_descriptions[3].binding = 0;
        attribute_descriptions[3].location = 3;
        attribute_descriptions[3].format = vk::Format::eR32G32Sfloat;
        attribute_descriptions[3].offset = offsetof(D3M::Vertex, uv);

        return attribute_descriptions;
    }

    D3M::D3M(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
    {
        assert(*(uint32_t*)buffer == 6);
        //numimg buffer + 0x04
        //numnimg buffer + 0x05
        num_triangles = *(uint16_t*)(buffer + 0x06);
        //numtri1 buffer + 0x08
        //numtri2 buffer + 0x0A
        //numtri3 buffer + 0x0C
        texture_name = std::string((char*)buffer + 0x0E, 16);
        auto vertices = (DatVertex*)(buffer + 0x1E);
        for (size_t i = 0; i < num_triangles * 3; ++i)
        {
            glm::vec4 color{ (vertices[i].color & 0xFF) / 255.0, ((vertices[i].color & 0xFF00) >> 8) / 255.0, ((vertices[i].color & 0xFF0000) >> 16) / 255.0, ((vertices[i].color & 0xFF000000) >> 24) / 255.0 };
            vertex_buffer.push_back({ vertices[i].pos, vertices[i].normal, color, vertices[i].uv });
        }
    }

    lotus::Task<> D3MLoader::LoadModel(std::shared_ptr<lotus::Model> model, lotus::Engine* engine, D3M* d3m)
    {
        model->lifetime = lotus::Lifetime::Short;
        std::vector<uint8_t> vertices(d3m->num_triangles * sizeof(D3M::Vertex) * 3);
        memcpy(vertices.data(), d3m->vertex_buffer.data(), d3m->vertex_buffer.size() * sizeof(D3M::Vertex));

        vk::BufferUsageFlags vertex_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
        vk::BufferUsageFlags index_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;
        vk::BufferUsageFlags aabbs_usage_flags = vk::BufferUsageFlagBits::eTransferDst;

        if (engine->config->renderer.RaytraceEnabled())
        {
            vertex_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
            index_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
            aabbs_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        }

        //assume every particle billboards (since it's set per generator, not per model)
        float max_dist = 0;
        for (const auto& vertex : d3m->vertex_buffer)
        {
            auto len = glm::length(vertex.pos);
            if (len > max_dist)
                max_dist = len;
        }

        auto mesh = std::make_unique<lotus::Mesh>(); 
        mesh->has_transparency = true;

        std::shared_ptr<lotus::Buffer> material_buffer = engine->renderer->gpu->memory_manager->GetBuffer(lotus::Material::getMaterialBufferSize(engine),
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
        std::shared_ptr<lotus::Texture> texture = lotus::Texture::getTexture(d3m->texture_name);
        if (!texture) texture = lotus::Texture::getTexture("default");
        mesh->material = co_await lotus::Material::make_material(engine, material_buffer, 0, texture);

        mesh->vertex_buffer = engine->renderer->gpu->memory_manager->GetBuffer(vertices.size(), vertex_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->index_buffer = engine->renderer->gpu->memory_manager->GetBuffer(d3m->num_triangles * 3 * sizeof(uint16_t), index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->aabbs_buffer = engine->renderer->gpu->memory_manager->GetBuffer(sizeof(vk::AabbPositionsKHR), aabbs_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->setIndexCount(d3m->num_triangles * 3);
        mesh->setVertexCount(d3m->num_triangles * 3);
        mesh->setMaxIndex(mesh->getIndexCount() - 1);
        mesh->setVertexInputAttributeDescription(getAttributeDescriptions());
        mesh->setVertexInputBindingDescription(getBindingDescriptions());
        mesh->pipeline = pipeline;

        model->meshes.push_back(std::move(mesh));

        co_await model->InitWork(engine, std::move(vertices), sizeof(D3M::Vertex), max_dist);
    }

    void D3MLoader::InitPipeline(lotus::Engine* engine)
    {
        auto vertex_module = engine->renderer->getShader("shaders/d3m_gbuffer_vert.spv");
        auto fragment_module = engine->renderer->getShader("shaders/particle_blend.spv");

        vk::PipelineShaderStageCreateInfo vert_shader_stage_info;
        vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
        vert_shader_stage_info.module = *vertex_module;
        vert_shader_stage_info.pName = "main";

        vk::PipelineShaderStageCreateInfo frag_shader_stage_info;
        frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
        frag_shader_stage_info.module = *fragment_module;
        frag_shader_stage_info.pName = "main";

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {vert_shader_stage_info, frag_shader_stage_info};

        vk::PipelineVertexInputStateCreateInfo vertex_input_info;

        auto binding_descriptions = getBindingDescriptions();
        auto attribute_descriptions = getAttributeDescriptions();

        vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_descriptions.size());
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexBindingDescriptions = binding_descriptions.data();
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

        vk::PipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
        input_assembly.primitiveRestartEnable = false;

        vk::Viewport viewport;
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)engine->renderer->swapchain->extent.width;
        viewport.height = (float)engine->renderer->swapchain->extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vk::Rect2D scissor;
        scissor.offset = vk::Offset2D{0, 0};
        scissor.extent = engine->renderer->swapchain->extent;

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
        depth_stencil.depthWriteEnable = false;
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

        vk::GraphicsPipelineCreateInfo pipeline_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.basePipelineHandle = nullptr;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipeline_info.pStages = shaderStages.data();
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pColorBlendState = &color_blending_subpass1;
        pipeline_info.subpass = 1;

        pipeline = engine->renderer->createGraphicsPipeline(pipeline_info);
    }
}

