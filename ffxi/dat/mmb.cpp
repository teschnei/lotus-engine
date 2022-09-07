#include "mmb.h"
#include "key_tables.h"
#include <list>
#include <ranges>
#include "engine/core.h"
#include "engine/renderer/model.h"
#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/component/instanced_models_component.h"

namespace FFXI
{

#pragma pack(push,1)

    struct SMMBHEAD {
        char id[3];
        long   type:7;
        long   next:19;
        long   is_shadow:1;
        long   is_extracted:1;
        long   ver_num:3;
        long   is_virtual:1;
        char unk[9];
    };

    struct SMMBHEAD2 {
        unsigned int MMBSize : 24;
        unsigned int d1 : 8;
        unsigned short d3 : 8;			//vertice stride (SMMBBlockVertex or SMMBBlockVertex2)
        unsigned short d4 : 8;			//d4, d5, d6 seems to be global across all Dat
        unsigned short d5 : 8;			//value is always the same (eg. moonshpere is always 240, 0, 0)
        unsigned short d6 : 8;			//d4, d5 incr the same as well
        char name[8];
    };
#pragma pack(pop)

    struct SMMBHeader {
        char imgID[16];
        int pieces;			//No of BlockHeader
        float x1,x2;		//BoundingRec Combine all BlockHeader (min,max)
        float y1,y2;
        float z1,z2;
        unsigned int offsetBlockHeader;	//offset to first SMMBBlockHeader
    };

    struct SMMBBlockHeader {
        int numModel;		//no of model block
        float x1,x2;		//BoundingRec Single (min,max)
        float y1,y2;
        float z1,z2;
        int numFace;
    };

    struct SMMBModelHeader {
        char textureName[16];
        unsigned short vertexsize;			//No of SMMBBlockVertex
        unsigned short blending;
    };

    struct SMMBBlockVertex {
        float x,y,z;
        float hx,hy,hz;
        unsigned int color;
        float u, v;
    };

    struct SMMBBlockVertex2 {
        float x,y,z;
        float dx,dy,dz;		//displacement?? cloth
        float hx,hy,hz;
        unsigned int color;
        float u, v;
    };

    inline std::vector<vk::VertexInputBindingDescription> getMMBBindingDescriptions()
    {
        std::vector<vk::VertexInputBindingDescription> binding_descriptions(2);

        binding_descriptions[0].binding = 0;
        binding_descriptions[0].stride = sizeof(FFXI::MMB::Vertex);
        binding_descriptions[0].inputRate = vk::VertexInputRate::eVertex;

        binding_descriptions[1].binding = 1;
        binding_descriptions[1].stride = sizeof(lotus::Component::InstancedModelsComponent::InstanceInfo);
        binding_descriptions[1].inputRate = vk::VertexInputRate::eInstance;

        return binding_descriptions;
    }

    inline std::vector<vk::VertexInputAttributeDescription> getMMBAttributeDescriptions()
    {
        std::vector<vk::VertexInputAttributeDescription> attribute_descriptions(15);

        attribute_descriptions[0].binding = 0;
        attribute_descriptions[0].location = 0;
        attribute_descriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[0].offset = offsetof(FFXI::MMB::Vertex, pos);

        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[1].offset = offsetof(FFXI::MMB::Vertex, normal);

        attribute_descriptions[2].binding = 0;
        attribute_descriptions[2].location = 2;
        attribute_descriptions[2].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[2].offset = offsetof(FFXI::MMB::Vertex, color);

        attribute_descriptions[3].binding = 0;
        attribute_descriptions[3].location = 3;
        attribute_descriptions[3].format = vk::Format::eR32G32Sfloat;
        attribute_descriptions[3].offset = offsetof(FFXI::MMB::Vertex, tex_coord);

        attribute_descriptions[4].binding = 1;
        attribute_descriptions[4].location = 4;
        attribute_descriptions[4].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[4].offset = offsetof(lotus::Component::InstancedModelsComponent::InstanceInfo, model);

        attribute_descriptions[5].binding = 1;
        attribute_descriptions[5].location = 5;
        attribute_descriptions[5].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[5].offset = offsetof(lotus::Component::InstancedModelsComponent::InstanceInfo, model) + sizeof(float) * 4;

        attribute_descriptions[6].binding = 1;
        attribute_descriptions[6].location = 6;
        attribute_descriptions[6].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[6].offset = offsetof(lotus::Component::InstancedModelsComponent::InstanceInfo, model) + sizeof(float) * 8;

        attribute_descriptions[7].binding = 1;
        attribute_descriptions[7].location = 7;
        attribute_descriptions[7].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[7].offset = offsetof(lotus::Component::InstancedModelsComponent::InstanceInfo, model) + sizeof(float) * 12;

        attribute_descriptions[8].binding = 1;
        attribute_descriptions[8].location = 8;
        attribute_descriptions[8].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[8].offset = offsetof(lotus::Component::InstancedModelsComponent::InstanceInfo, model_t);

        attribute_descriptions[9].binding = 1;
        attribute_descriptions[9].location = 9;
        attribute_descriptions[9].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[9].offset = offsetof(lotus::Component::InstancedModelsComponent::InstanceInfo, model_t) + sizeof(float) * 4;

        attribute_descriptions[10].binding = 1;
        attribute_descriptions[10].location = 10;
        attribute_descriptions[10].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[10].offset = offsetof(lotus::Component::InstancedModelsComponent::InstanceInfo, model_t) + sizeof(float) * 8;

        attribute_descriptions[11].binding = 1;
        attribute_descriptions[11].location = 11;
        attribute_descriptions[11].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[11].offset = offsetof(lotus::Component::InstancedModelsComponent::InstanceInfo, model_t) + sizeof(float) * 12;

        attribute_descriptions[12].binding = 1;
        attribute_descriptions[12].location = 12;
        attribute_descriptions[12].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[12].offset = offsetof(lotus::Component::InstancedModelsComponent::InstanceInfo, model_it);

        attribute_descriptions[13].binding = 1;
        attribute_descriptions[13].location = 13;
        attribute_descriptions[13].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[13].offset = offsetof(lotus::Component::InstancedModelsComponent::InstanceInfo, model_it) + sizeof(float) * 3;

        attribute_descriptions[14].binding = 1;
        attribute_descriptions[14].location = 14;
        attribute_descriptions[14].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[14].offset = offsetof(lotus::Component::InstancedModelsComponent::InstanceInfo, model_it) + sizeof(float) * 6;

        return attribute_descriptions;
    }

    MMB::MMB(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
    {
        size_t offset = 0;
        SMMBHEAD* head = (SMMBHEAD*)buffer;
        SMMBHEAD2* head2 = (SMMBHEAD2*)buffer;

        offset += sizeof(SMMBHEAD);

        uint32_t size = 0;

        if (head->id[0] == 'M' && head->id[1] == 'M' && head->id[2] == 'B')
        {
            size = head->next * 16;
            type = 1;
        }
        else
        {
            size = head2->MMBSize;
            type = 2;
        }

        SMMBHeader* header = (SMMBHeader*)(buffer + offset);

        memcpy(name, header->imgID, 16);

        offset += sizeof(SMMBHeader);

        std::list<size_t> offset_list;
        uint32_t piece_offset;
        if (header->offsetBlockHeader == 0)
        {
            if (header->pieces != 0)
            {
                for (int i = 0; i < 8; ++i)
                {
                    piece_offset = *(uint32_t*)(buffer + offset);
                    if (piece_offset != 0)
                    {
                        offset_list.push_back(piece_offset);
                    }
                    offset += sizeof(uint32_t);
                }
            }
            else
            {
                offset_list.push_back(offset);
            }
        }
        else
        {
            offset_list.push_back(header->offsetBlockHeader);
            uint32_t max = static_cast<uint32_t>(header->offsetBlockHeader - offset);
            if (max > 0)
            {
                for (size_t i=0; i < max; i+=sizeof(uint32_t))
                {
                    piece_offset = *(uint32_t*)(buffer + offset);
                    if (piece_offset != 0)
                    {
                        offset_list.push_back(piece_offset);
                    }
                    offset += sizeof(uint32_t);
                }
                if (offset_list.size() != header->pieces)
                {
                    DEBUG_BREAK();
                }
            }
            offset += header->offsetBlockHeader;
        }

        for (int piece = 0; piece < header->pieces; ++piece)
        {
            if (!offset_list.empty())
            {
                offset = offset_list.front();
                offset_list.pop_front();
            }

            SMMBBlockHeader* block_header = (SMMBBlockHeader*)(buffer + offset);
            offset += sizeof(SMMBBlockHeader);

            if (block_header->numModel > 50)
            {
                DEBUG_BREAK();
            }
            for (int model_index = 0; model_index < block_header->numModel; ++model_index)
            {
                if (offset + sizeof(SMMBModelHeader) > len)
                {
                    break;
                }
                SMMBModelHeader* model_header = (SMMBModelHeader*)(buffer + offset);
                offset += sizeof(SMMBModelHeader);

                Mesh mesh;
                mesh.blending = model_header->blending;
                memcpy(mesh.textureName, model_header->textureName, 16);

                for (int vert_index = 0; vert_index < model_header->vertexsize; ++vert_index)
                {
                    Vertex vertex{};
                    if (head2->d3 == 2)
                    {
                        SMMBBlockVertex2* vertex_head = (SMMBBlockVertex2*)(buffer + offset);
                        vertex.pos = { vertex_head->x, vertex_head->y, vertex_head->z };
                        vertex.normal = { vertex_head->hx, vertex_head->hy, vertex_head->hz };
                        vertex.color = { ((vertex_head->color & 0xFF0000) >> 16)/128.f, ((vertex_head->color & 0xFF00) >> 8)/128.f, (vertex_head->color & 0xFF)/128.f, ((vertex_head->color & 0xFF000000) >> 24) / 128.f};
                        vertex.tex_coord = { vertex_head->u, vertex_head->v };
                        offset += sizeof(SMMBBlockVertex2);
                    }
                    else
                    {
                        SMMBBlockVertex* vertex_head = (SMMBBlockVertex*)(buffer + offset);
                        vertex.pos = { vertex_head->x, vertex_head->y, vertex_head->z };
                        vertex.normal = { vertex_head->hx, vertex_head->hy, vertex_head->hz };
                        vertex.color = { ((vertex_head->color & 0xFF0000) >> 16)/128.f, ((vertex_head->color & 0xFF00) >> 8)/128.f, (vertex_head->color & 0xFF)/128.f, ((vertex_head->color & 0xFF000000) >> 24) / 128.f};
                        vertex.tex_coord = { vertex_head->u, vertex_head->v };
                        offset += sizeof(SMMBBlockVertex);
                    }
                    //displace vertices slightly because RTX can't use mesh order to determine z-fighting
                    float mesh_offset = 0.00001f;
                    glm::vec3 mesh_scale = glm::vec3(vertex.normal.x * mesh_offset * model_index, vertex.normal.y * mesh_offset * model_index, vertex.normal.z * mesh_offset * model_index);
                    vertex.pos += mesh_scale;
                    mesh.vertices.push_back(std::move(vertex));
                }

                uint16_t num_indices = *(uint16_t*)(buffer + offset);
                offset += 4;

                if ((head->id[0] == 'M' && head->id[1] == 'M' && head->id[2] == 'B') || (head->id[0] != 'M' && head2->d3 == 2))
                {
                    mesh.topology = vk::PrimitiveTopology::eTriangleList;
                    for (int i = 0; i < num_indices; ++i)
                    {
                        mesh.indices.push_back(*(uint16_t*)(buffer + offset));
                        offset += sizeof(uint16_t);
                    }
                }
                else
                {
                    //mesh.topology = vk::PrimitiveTopology::eTriangleStrip;
                    mesh.topology = vk::PrimitiveTopology::eTriangleList;
                    for (int i = 0; i < num_indices - 2; ++i)
                    {
                        auto i1 = *(uint16_t*)(buffer + offset);
                        auto i2 = *(uint16_t*)(buffer + offset + sizeof(uint16_t));
                        auto i3 = *(uint16_t*)(buffer + offset + sizeof(uint16_t) * 2);

                        if (i1 != i2 && i2 != i3)
                        {
                            if (i % 2)
                            {
                                mesh.indices.push_back(i2);
                                mesh.indices.push_back(i1);
                                mesh.indices.push_back(i3);
                            }
                            else
                            {
                                mesh.indices.push_back(i1);
                                mesh.indices.push_back(i2);
                                mesh.indices.push_back(i3);
                            }
                        }
                        offset += sizeof(uint16_t);
                    }
                    offset += sizeof(uint16_t) * 2;
                }
                if (num_indices % 2 != 0)
                    offset += sizeof(uint16_t);

                if (!mesh.indices.empty())
                    meshes.push_back(std::move(mesh));
            }
        }
    }

    bool MMB::DecodeMMB(uint8_t* buffer, size_t max_len)
    {
        if (buffer[3] >= 5)
        {
            uint32_t len = *(uint32_t*)buffer & 0x00FFFFFF;
            uint32_t key = key_table[buffer[5] ^ 0xF0];
            int key_count = 0;
            for (uint32_t pos = 8; pos < len; ++pos)
            {
                uint32_t x = ((key & 0xFF) << 8) | (key & 0xFF);
                key += ++key_count;

                buffer[pos] ^= (x >> (key & 7));
                key += ++key_count;
            }
        }

        if (buffer[6] == 0xFF && buffer[7] == 0xFF)
        {
            uint32_t len = *(uint32_t*)buffer & 0x00FFFFFF;
            uint32_t key1 = buffer[5] ^ 0xF0;
            uint32_t key2 = key_table2[key1];
            int key_count = 0;

            uint32_t decode_count = ((len - 8) & ~0xF) / 2;

            uint32_t* data1 = (uint32_t*)(buffer + 8);
            uint32_t* data2 = (uint32_t*)(buffer + 8 + decode_count);

            for(uint32_t pos = 0; pos < decode_count; pos += 8)
            {
                if (key2 & 1)
                {
                    uint32_t tmp;
                    tmp = data1[0];
                    data1[0] = data2[0];
                    data2[0] = tmp;

                    tmp = data1[1];
                    data1[1] = data2[1];
                    data2[1] = tmp;
                }
                key1 += 9;
                key2 += key1;
                data1 += 2;
                data2 += 2;
            }
        }
        return true;
    }

    lotus::Task<> MMBLoader::LoadModel(std::shared_ptr<lotus::Model> model, lotus::Engine* engine, MMB* mmb)
    {
        if (!pipeline_flag.test_and_set())
        {
            InitPipeline(engine);
            pipeline_latch.count_down();
        }
        pipeline_latch.wait();
        model->light_offset = 1;
        model->is_static = true;
        std::vector<std::vector<uint8_t>> vertices;
        std::vector<std::vector<uint8_t>> indices;
        std::map<lotus::Mesh*, lotus::WorkerTask<std::shared_ptr<lotus::Material>>> material_map;
        std::shared_ptr<lotus::Buffer> material_buffer;
        if (mmb->meshes.size() > 0)
        {
            material_buffer = engine->renderer->gpu->memory_manager->GetBuffer(lotus::Material::getMaterialBufferSize(engine) * mmb->meshes.size(),
                vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
        }
        uint32_t material_buffer_offset = 0;

        for (const auto& mmb_mesh : mmb->meshes)
        {
            auto mesh = std::make_unique<lotus::Mesh>();
            std::shared_ptr<lotus::Texture> texture = lotus::Texture::getTexture(mmb_mesh.textureName);
            if (!texture) texture = lotus::Texture::getTexture("default");
            material_map.insert(std::make_pair(mesh.get(), lotus::Material::make_material(engine, material_buffer, material_buffer_offset, texture, 1)));
            material_buffer_offset += lotus::Material::getMaterialBufferSize(engine);

            mesh->setVertexInputAttributeDescription(getMMBAttributeDescriptions(), sizeof(FFXI::MMB::Vertex));
            mesh->setVertexInputBindingDescription(getMMBBindingDescriptions());
            mesh->setIndexCount(static_cast<int>(mmb_mesh.indices.size()));
            mesh->has_transparency = mmb_mesh.blending & 0x8000 || mmb->name[0] == '_';
            mesh->pipelines.push_back(mesh->has_transparency ? pipeline_blend : pipeline);
            mesh->pipelines.push_back(mesh->has_transparency ? pipeline_shadowmap_blend : pipeline_shadowmap);
            mesh->blending = mmb_mesh.blending;

            std::vector<uint8_t> vertices_uint8;
            vertices_uint8.resize(mmb_mesh.vertices.size() * sizeof(FFXI::MMB::Vertex));
            memcpy(vertices_uint8.data(), mmb_mesh.vertices.data(), vertices_uint8.size());

            std::vector<uint8_t> indices_uint8;
            indices_uint8.resize(mmb_mesh.indices.size() * sizeof(uint16_t));
            memcpy(indices_uint8.data(), mmb_mesh.indices.data(), indices_uint8.size());

            auto vertex_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
            auto index_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;

            if (engine->config->renderer.RaytraceEnabled())
            {
                vertex_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
                index_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
            }

            mesh->vertex_buffer = engine->renderer->gpu->memory_manager->GetBuffer(vertices_uint8.size(), vertex_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
            mesh->index_buffer = engine->renderer->gpu->memory_manager->GetBuffer(indices_uint8.size(), index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
            mesh->vertex_descriptor_index = engine->renderer->global_descriptors->getVertexIndex();
            mesh->vertex_descriptor_index->write({
                .buffer = mesh->vertex_buffer->buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE
            });

            mesh->index_descriptor_index = engine->renderer->global_descriptors->getIndexIndex();
            mesh->index_descriptor_index->write({
                .buffer = mesh->index_buffer->buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE
            });

            mesh->setMaxIndex(*std::ranges::max_element(mmb_mesh.indices));

            vertices.push_back(std::move(vertices_uint8));
            indices.push_back(std::move(indices_uint8));

            model->meshes.push_back(std::move(mesh));
        }
        for (auto& [mesh, task] : material_map)
        {
            mesh->material = co_await task;
        }
        model->lifetime = lotus::Lifetime::Long;
        co_await model->InitWork(engine, std::move(vertices), std::move(indices), sizeof(MMB::Vertex));
    }

    void MMBLoader::InitPipeline(lotus::Engine* engine)
    {
        auto vertex_module = engine->renderer->getShader("shaders/mmb_gbuffer_vert.spv");
        auto fragment_module = engine->renderer->getShader("shaders/gbuffer_frag.spv");

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

        auto binding_descriptions = getMMBBindingDescriptions();
        auto attribute_descriptions = getMMBAttributeDescriptions();

        vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_descriptions.size());
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexBindingDescriptions = binding_descriptions.data();
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

        vk::PipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
        input_assembly.primitiveRestartEnable = false;

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
        color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;

        std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachment_states(7, color_blend_attachment);

        color_blend_attachment_states[3].blendEnable = true;

        vk::PipelineColorBlendStateCreateInfo color_blending;
        color_blending.logicOpEnable = false;
        color_blending.logicOp = vk::LogicOp::eCopy;
        color_blending.attachmentCount = static_cast<uint32_t>(color_blend_attachment_states.size());
        color_blending.pAttachments = color_blend_attachment_states.data();
        color_blending.blendConstants[0] = 0.0f;
        color_blending.blendConstants[1] = 0.0f;
        color_blending.blendConstants[2] = 0.0f;
        color_blending.blendConstants[3] = 0.0f;

        std::vector<vk::DynamicState> dynamic_states = { vk::DynamicState::eScissor, vk::DynamicState::eViewport };
        vk::PipelineDynamicStateCreateInfo dynamic_state_ci
        {
            .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data()
        };

        vk::PipelineViewportStateCreateInfo viewport_info
        {
            .viewportCount = 1,
            .scissorCount = 1
        };

        vk::GraphicsPipelineCreateInfo pipeline_info
        {
            .stageCount = static_cast<uint32_t>(shaderStages.size()),
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertex_input_info,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport_info,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depth_stencil,
            .pColorBlendState = &color_blending,
            .pDynamicState = &dynamic_state_ci,
            .basePipelineHandle = nullptr
        };

        pipeline = engine->renderer->createGraphicsPipeline(pipeline_info);

        fragment_module = engine->renderer->getShader("shaders/blend.spv");

        frag_shader_stage_info.module = *fragment_module;

        shaderStages[1] = frag_shader_stage_info;

        pipeline_blend = engine->renderer->createGraphicsPipeline(pipeline_info);

        color_blending.attachmentCount = 0;

        vertex_module = engine->renderer->getShader("shaders/mmb_shadow_vert.spv");
        fragment_module = engine->renderer->getShader("shaders/shadow_frag.spv");

        vert_shader_stage_info.module = *vertex_module;
        frag_shader_stage_info.module = *fragment_module;

        shaderStages = { vert_shader_stage_info, frag_shader_stage_info };

        pipeline_info.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipeline_info.pStages = shaderStages.data();

        rasterizer.depthClampEnable = true;
        rasterizer.depthBiasEnable = true;

        dynamic_states.push_back(vk::DynamicState::eDepthBias);
        dynamic_state_ci.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state_ci.pDynamicStates = dynamic_states.data();
        pipeline_shadowmap_blend = engine->renderer->createShadowmapPipeline(pipeline_info);

        pipeline_info.stageCount = 1;
        pipeline_info.pStages = &vert_shader_stage_info;
        pipeline_shadowmap = engine->renderer->createShadowmapPipeline(pipeline_info);
    }
}
