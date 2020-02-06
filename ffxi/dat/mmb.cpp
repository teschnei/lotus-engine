#include "mmb.h"
#include "key_tables.h"
#include <list>
#include "engine/core.h"
#include "engine/entity/landscape_entity.h"
#include "engine/renderer/model.h"
#include "engine/task/model_init.h"

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

    std::vector<vk::VertexInputBindingDescription> MMB::Vertex::getBindingDescriptions() {
        std::vector<vk::VertexInputBindingDescription> binding_descriptions(2);

        binding_descriptions[0].binding = 0;
        binding_descriptions[0].stride = sizeof(Vertex);
        binding_descriptions[0].inputRate = vk::VertexInputRate::eVertex;

        binding_descriptions[1].binding = 1;
        binding_descriptions[1].stride = sizeof(lotus::LandscapeEntity::InstanceInfo);
        binding_descriptions[1].inputRate = vk::VertexInputRate::eInstance;

        return binding_descriptions;
    }

    std::vector<vk::VertexInputAttributeDescription> MMB::Vertex::getAttributeDescriptions() {
        std::vector<vk::VertexInputAttributeDescription> attribute_descriptions(11);

        attribute_descriptions[0].binding = 0;
        attribute_descriptions[0].location = 0;
        attribute_descriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[0].offset = offsetof(Vertex, pos);

        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[1].offset = offsetof(Vertex, normal);

        attribute_descriptions[2].binding = 0;
        attribute_descriptions[2].location = 2;
        attribute_descriptions[2].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[2].offset = offsetof(Vertex, color);

        attribute_descriptions[3].binding = 0;
        attribute_descriptions[3].location = 3;
        attribute_descriptions[3].format = vk::Format::eR32G32Sfloat;
        attribute_descriptions[3].offset = offsetof(Vertex, tex_coord);

        attribute_descriptions[4].binding = 1;
        attribute_descriptions[4].location = 4;
        attribute_descriptions[4].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[4].offset = 0;

        attribute_descriptions[5].binding = 1;
        attribute_descriptions[5].location = 5;
        attribute_descriptions[5].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[5].offset = sizeof(float)*4;

        attribute_descriptions[6].binding = 1;
        attribute_descriptions[6].location = 6;
        attribute_descriptions[6].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[6].offset = sizeof(float)*8;

        attribute_descriptions[7].binding = 1;
        attribute_descriptions[7].location = 7;
        attribute_descriptions[7].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[7].offset = sizeof(float)*12;

        attribute_descriptions[8].binding = 1;
        attribute_descriptions[8].location = 8;
        attribute_descriptions[8].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[8].offset = sizeof(float)*16;

        attribute_descriptions[9].binding = 1;
        attribute_descriptions[9].location = 9;
        attribute_descriptions[9].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[9].offset = sizeof(float)*20;

        attribute_descriptions[10].binding = 1;
        attribute_descriptions[10].location = 10;
        attribute_descriptions[10].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[10].offset = sizeof(float)*24;

        return attribute_descriptions;
    }

    MMB::MMB(uint8_t* buffer, size_t max_len, bool offset_vertices)
    {
        size_t offset = 0;
        SMMBHEAD* head = (SMMBHEAD*)buffer;
        SMMBHEAD2* head2 = (SMMBHEAD2*)buffer;

        offset += sizeof(SMMBHEAD);

        uint32_t size = 0;
        uint8_t type = 0;

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
                    __debugbreak();
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
                __debugbreak();
            }
            for (int model_index = 0; model_index < block_header->numModel; ++model_index)
            {
                if (offset + sizeof(SMMBModelHeader) > max_len)
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
                        vertex.color = { ((vertex_head->color & 0xFF0000) >> 16)/256.f, ((vertex_head->color & 0xFF00) >> 8)/256.f, (vertex_head->color & 0xFF)/256.f};
                        vertex.tex_coord = { vertex_head->u, vertex_head->v };
                        offset += sizeof(SMMBBlockVertex2);
                    }
                    else
                    {
                        SMMBBlockVertex* vertex_head = (SMMBBlockVertex*)(buffer + offset);
                        vertex.pos = { vertex_head->x, vertex_head->y, vertex_head->z };
                        vertex.normal = { vertex_head->hx, vertex_head->hy, vertex_head->hz };
                        vertex.color = { ((vertex_head->color & 0xFF0000) >> 16)/256.f, ((vertex_head->color & 0xFF00) >> 8)/256.f, (vertex_head->color & 0xFF)/256.f};
                        vertex.tex_coord = { vertex_head->u, vertex_head->v };
                        offset += sizeof(SMMBBlockVertex);
                    }
                    //displace vertices slightly because RTX can't use mesh order to determine z-fighting
                    float mesh_offset = 0.00000f;
                    if (offset_vertices)
                        mesh_offset = 0.00001f;
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

    MMBLoader::MMBLoader(MMB* _mmb) : ModelLoader(), mmb(_mmb) {}

    void MMBLoader::LoadModel(std::shared_ptr<lotus::Model>& model)
    {
        std::vector<std::vector<uint8_t>> vertices;
        std::vector<std::vector<uint8_t>> indices;
        for (const auto& mmb_mesh : mmb->meshes)
        {
            auto mesh = std::make_unique<lotus::Mesh>();
            mesh->texture = lotus::Texture::getTexture(mmb_mesh.textureName);

            mesh->setVertexInputAttributeDescription(FFXI::MMB::Vertex::getAttributeDescriptions());
            mesh->setVertexInputBindingDescription(FFXI::MMB::Vertex::getBindingDescriptions());
            mesh->setIndexCount(static_cast<int>(mmb_mesh.indices.size()));
            mesh->has_transparency = mmb_mesh.blending & 0x8000 || mmb->name[0] == '_';
            mesh->blending = mmb_mesh.blending;

            std::vector<uint8_t> vertices_uint8;
            vertices_uint8.resize(mmb_mesh.vertices.size() * sizeof(FFXI::MMB::Vertex));
            memcpy(vertices_uint8.data(), mmb_mesh.vertices.data(), vertices_uint8.size());

            std::vector<uint8_t> indices_uint8;
            indices_uint8.resize(mmb_mesh.indices.size() * sizeof(uint16_t));
            memcpy(indices_uint8.data(), mmb_mesh.indices.data(), indices_uint8.size());

            auto vertex_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
            auto index_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;

            if (engine->renderer.RTXEnabled())
            {
                vertex_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer;
                index_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer;
            }

            mesh->vertex_buffer = engine->renderer.memory_manager->GetBuffer(vertices_uint8.size(), vertex_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
            mesh->index_buffer = engine->renderer.memory_manager->GetBuffer(indices_uint8.size(), index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);

            vertices.push_back(std::move(vertices_uint8));
            indices.push_back(std::move(indices_uint8));

            model->meshes.push_back(std::move(mesh));
        }
        model->lifetime = lotus::Lifetime::Long;
        engine->worker_pool.addWork(std::make_unique<lotus::ModelInitTask>(engine->renderer.getCurrentImage(), model, std::move(vertices), std::move(indices), sizeof(MMB::Vertex)));
    }
}
