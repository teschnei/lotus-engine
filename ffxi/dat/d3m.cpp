#include "d3m.h"

#include <numeric>
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"

namespace FFXI
{
    struct DatVertexD3M
    {
        glm::vec3 pos;
        glm::vec3 normal;
        uint32_t color;
        glm::vec2 uv;
    };

    struct DatVertexD3A
    {
        glm::vec3 pos;
        uint32_t color;
        glm::vec2 uv;
    };

    struct DatRectD3A
    {
        uint16_t unk1;
        uint16_t unk2;
        DatVertexD3A vertices[6];
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
        auto vertices = (DatVertexD3M*)(buffer + 0x1E);
        for (size_t i = 0; i < num_triangles * 3; ++i)
        {
            glm::vec4 color{ ((vertices[i].color & 0xFF0000) >> 16) / 128.f, ((vertices[i].color & 0xFF00) >> 8) / 128.f, ((vertices[i].color & 0xFF)) / 128.f, ((vertices[i].color & 0xFF000000) >> 24) / 128.f };
            vertex_buffer.push_back({ vertices[i].pos, vertices[i].normal, color, vertices[i].uv });
        }
    }

    D3A::D3A(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
    {
        num_quads = *(uint16_t*)(buffer + 0x02);
        DEBUG_BREAK_IF(num_quads * sizeof(DatRectD3A) + 0x18 > (len));
        //not sure what the other values are? num_quads seems to be repeated at 0x05 and 0x06 (but doesn't do anything), and 0x07 is 1?
        texture_name = std::string((char*)buffer + 0x08, 16);
        //i think a d3a is always a square and the number of quads is "frames" of an animation?
        auto rects = (DatRectD3A*)(buffer + 0x18);
        for (size_t i = 0; i < num_quads; ++i)
        {
            for (size_t j = 0; j < 6; ++j)
            {
                glm::vec4 color{ ((rects[i].vertices[j].color & 0xFF0000) >> 16) / 128.f, ((rects[i].vertices[j].color & 0xFF00) >> 8) / 128.f, ((rects[i].vertices[j].color & 0xFF)) / 128.f, ((rects[i].vertices[j].color & 0xFF000000) >> 24) / 128.f };
                vertex_buffer.push_back({ rects[i].vertices[j].pos, glm::vec3(0.f,0.f,1.f), color, rects[i].vertices[j].uv });
            }
        }
    }

    lotus::Task<> D3MLoader::LoadModelAABB(std::shared_ptr<lotus::Model> model, lotus::Engine* engine,
        std::vector<D3M::Vertex>& vertices, std::vector<uint16_t>& indices, std::shared_ptr<lotus::Texture> texture, uint16_t sprite_count)
    {
        if (!pipeline_flag.test_and_set())
        {
            co_await InitPipeline(engine);
            pipeline_latch.count_down();
        }
        pipeline_latch.wait();
        model->lifetime = lotus::Lifetime::Short;
        std::vector<uint8_t> vertices_uint8(vertices.size() * sizeof(D3M::Vertex));
        memcpy(vertices_uint8.data(), vertices.data(), vertices.size() * sizeof(D3M::Vertex));

        vk::BufferUsageFlags vertex_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
        vk::BufferUsageFlags index_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;
        vk::BufferUsageFlags aabbs_usage_flags = vk::BufferUsageFlagBits::eTransferDst;

        if (engine->config->renderer.RaytraceEnabled())
        {
            vertex_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
            index_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
            aabbs_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
        }

        auto mesh = std::make_unique<lotus::Mesh>(); 
        mesh->has_transparency = true;

        std::shared_ptr<lotus::Buffer> material_buffer = engine->renderer->gpu->memory_manager->GetBuffer(lotus::Material::getMaterialBufferSize(engine),
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
        if (!texture)
            texture = blank_texture;
        mesh->material = co_await lotus::Material::make_material(engine, material_buffer, 0, texture);

        mesh->vertex_buffer = engine->renderer->gpu->memory_manager->GetBuffer(vertices_uint8.size(), vertex_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->index_buffer = engine->renderer->gpu->memory_manager->GetBuffer(indices.size() * sizeof(uint16_t), index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->aabbs_buffer = engine->renderer->gpu->memory_manager->GetBuffer(sizeof(vk::AabbPositionsKHR), aabbs_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->setIndexCount(indices.size());
        mesh->setVertexCount(vertices.size());
        mesh->setMaxIndex(vertices.size() - 1);
        mesh->setVertexInputAttributeDescription(getAttributeDescriptions(), sizeof(D3M::Vertex));
        mesh->setVertexInputBindingDescription(getBindingDescriptions());
        mesh->setSpriteCount(sprite_count);
        mesh->pipelines.push_back(pipeline_add);
        mesh->pipelines.push_back(nullptr);
        mesh->pipelines.push_back(pipeline_blend);
        mesh->pipelines.push_back(pipeline_sub);

        model->meshes.push_back(std::move(mesh));

        //assume every particle billboards (since it's set per generator, not per model)
        float max_dist = 0;
        for (const auto& vertex : vertices)
        {
            auto len = glm::length(vertex.pos);
            if (len > max_dist)
                max_dist = len;
        }

        co_await model->InitWorkAABB(engine, std::move(vertices_uint8), std::move(indices), sizeof(D3M::Vertex), max_dist);
    }

    lotus::Task<> D3MLoader::LoadModelTriangle(std::shared_ptr<lotus::Model> model, lotus::Engine* engine,
        std::vector<D3M::Vertex>& vertices, std::vector<uint16_t>& indices, std::shared_ptr<lotus::Texture> texture, uint16_t sprite_count)
    {
        if (!pipeline_flag.test_and_set())
        {
            co_await InitPipeline(engine);
            pipeline_latch.count_down();
        }
        pipeline_latch.wait();
        model->lifetime = lotus::Lifetime::Short;
        std::vector<uint8_t> vertices_uint8(vertices.size() * sizeof(D3M::Vertex));
        memcpy(vertices_uint8.data(), vertices.data(), vertices.size() * sizeof(D3M::Vertex));
        std::vector<uint8_t> indices_uint8(indices.size() * sizeof(uint16_t));
        memcpy(indices_uint8.data(), indices.data(), indices.size() * sizeof(uint16_t));

        vk::BufferUsageFlags vertex_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
        vk::BufferUsageFlags index_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;

        if (engine->config->renderer.RaytraceEnabled())
        {
            vertex_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
            index_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
        }

        auto mesh = std::make_unique<lotus::Mesh>(); 
        mesh->has_transparency = true;

        std::shared_ptr<lotus::Buffer> material_buffer = engine->renderer->gpu->memory_manager->GetBuffer(lotus::Material::getMaterialBufferSize(engine),
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
        if (!texture)
            texture = blank_texture;
        mesh->material = co_await lotus::Material::make_material(engine, material_buffer, 0, texture);

        mesh->vertex_buffer = engine->renderer->gpu->memory_manager->GetBuffer(vertices_uint8.size(), vertex_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->index_buffer = engine->renderer->gpu->memory_manager->GetBuffer(indices.size() * sizeof(uint16_t), index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->setIndexCount(indices.size());
        mesh->setVertexCount(vertices.size());
        mesh->setMaxIndex(vertices.size() - 1);
        mesh->setVertexInputAttributeDescription(getAttributeDescriptions(), sizeof(D3M::Vertex));
        mesh->setVertexInputBindingDescription(getBindingDescriptions());
        mesh->setSpriteCount(sprite_count);
        mesh->pipelines.push_back(pipeline_add);
        mesh->pipelines.push_back(nullptr);
        mesh->pipelines.push_back(pipeline_blend);
        mesh->pipelines.push_back(pipeline_sub);

        model->meshes.push_back(std::move(mesh));

        std::vector<std::vector<uint8_t>> vertices_vector{ std::move(vertices_uint8) };
        std::vector<std::vector<uint8_t>> indices_vector{ std::move(indices_uint8) };

        co_await model->InitWork(engine, std::move(vertices_vector), std::move(indices_vector), sizeof(D3M::Vertex));
    }

    lotus::Task<> D3MLoader::LoadModelRing(std::shared_ptr<lotus::Model> model, lotus::Engine* engine,
        std::vector<D3M::Vertex>&& vertices, std::vector<uint16_t>&& indices)
    {
        co_await LoadModelTriangle(model, engine, vertices, indices, blank_texture, 1);
    }

    lotus::Task<> D3MLoader::LoadD3M(std::shared_ptr<lotus::Model> model, lotus::Engine* engine, D3M* d3m)
    {
        std::vector<uint16_t> index_buffer(d3m->vertex_buffer.size());
        std::iota(index_buffer.begin(), index_buffer.end(), 0);

        //co_await LoadModelAABB(model, engine, d3m->vertex_buffer, index_buffer, lotus::Texture::getTexture(d3m->texture_name));
        co_await LoadModelTriangle(model, engine, d3m->vertex_buffer, index_buffer, lotus::Texture::getTexture(d3m->texture_name), 1);
    }

    lotus::Task<> D3MLoader::LoadD3A(std::shared_ptr<lotus::Model> model, lotus::Engine* engine, D3A* d3a)
    {
        //TODO: this size may be from D3A somewhere (maybe that 0x01 that doesn't seem to do anything?)
        std::vector<uint16_t> index_buffer(6);
        std::iota(index_buffer.begin(), index_buffer.end(), 0);

        //co_await LoadModelAABB(model, engine, d3a->vertex_buffer, index_buffer, lotus::Texture::getTexture(d3a->texture_name));
        co_await LoadModelTriangle(model, engine, d3a->vertex_buffer, index_buffer, lotus::Texture::getTexture(d3a->texture_name), d3a->num_quads);
    }

    lotus::Task<> D3MLoader::InitPipeline(lotus::Engine* engine)
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

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = { vert_shader_stage_info, frag_shader_stage_info };

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
        scissor.offset = vk::Offset2D{ 0, 0 };
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
        vk::PipelineColorBlendAttachmentState color_blend_attachment_particle = color_blend_attachment;
        color_blend_attachment_particle.blendEnable = false;
        color_blend_attachment_particle.srcColorBlendFactor = vk::BlendFactor::eOne;
        color_blend_attachment_particle.dstColorBlendFactor = vk::BlendFactor::eOne;
        color_blend_attachment_particle.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        color_blend_attachment_particle.dstAlphaBlendFactor = vk::BlendFactor::eOne;

        std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachment_states(5, color_blend_attachment);
        std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachment_states_subpass1{ color_blend_attachment_accumulation, color_blend_attachment_revealage, color_blend_attachment_particle };

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

        pipeline_blend = engine->renderer->createGraphicsPipeline(pipeline_info);

        auto fragment_module_add = engine->renderer->getShader("shaders/particle_add.spv");
        frag_shader_stage_info.module = *fragment_module_add;
        shaderStages[1] = frag_shader_stage_info;

        color_blend_attachment_states_subpass1[0].blendEnable = false;
        color_blend_attachment_states_subpass1[1].blendEnable = false;
        color_blend_attachment_states_subpass1[2].blendEnable = true;
        pipeline_add = engine->renderer->createGraphicsPipeline(pipeline_info);

        color_blend_attachment_states_subpass1[2].colorBlendOp = vk::BlendOp::eSubtract;
        pipeline_sub = engine->renderer->createGraphicsPipeline(pipeline_info);

        blank_texture = co_await lotus::Texture::LoadTexture("d3m_blank", BlankTextureLoader::LoadTexture, engine);
    }

    lotus::Task<> D3MLoader::BlankTextureLoader::LoadTexture(std::shared_ptr<lotus::Texture>& texture, lotus::Engine* engine)
    {
        texture->setWidth(1);
        texture->setHeight(1);
        std::vector<uint8_t> texture_data{ 255, 255, 255, 255 };

        texture->image = engine->renderer->gpu->memory_manager->GetImage(texture->getWidth(), texture->getHeight(), vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = texture->image->image;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = vk::Format::eR8G8B8A8Unorm;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        texture->image_view = engine->renderer->gpu->device->createImageViewUnique(image_view_info, nullptr);

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

        texture->sampler = engine->renderer->gpu->device->createSamplerUnique(sampler_info, nullptr);

        std::vector<std::vector<uint8_t>> texture_datas{ std::move(texture_data) };
        co_await texture->Init(engine, std::move(texture_datas));
    }
}

