#include "d3m.h"

#include "engine/core.h"
#include "engine/task/particle_model_init.h"

namespace FFXI
{
    struct DatVertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        uint32_t color;
        glm::vec2 uv;
    };

    std::vector<vk::VertexInputBindingDescription> D3M::Vertex::getBindingDescriptions()
    {
        std::vector<vk::VertexInputBindingDescription> binding_descriptions(1);

        binding_descriptions[0].binding = 0;
        binding_descriptions[0].stride = sizeof(Vertex);
        binding_descriptions[0].inputRate = vk::VertexInputRate::eVertex;

        return binding_descriptions;
    }

    std::vector<vk::VertexInputAttributeDescription> D3M::Vertex::getAttributeDescriptions()
    {
        std::vector<vk::VertexInputAttributeDescription> attribute_descriptions(4);

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
        attribute_descriptions[2].format = vk::Format::eR32G32B32A32Sfloat;
        attribute_descriptions[2].offset = offsetof(Vertex, color);

        attribute_descriptions[3].binding = 0;
        attribute_descriptions[3].location = 3;
        attribute_descriptions[3].format = vk::Format::eR32G32Sfloat;
        attribute_descriptions[3].offset = offsetof(Vertex, uv);

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

    void D3MLoader::LoadModel(std::shared_ptr<lotus::Model>& model)
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

        mesh->texture = lotus::Texture::getTexture(d3m->texture_name);

        mesh->vertex_buffer = engine->renderer.gpu->memory_manager->GetBuffer(vertices.size(), vertex_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->index_buffer = engine->renderer.gpu->memory_manager->GetBuffer(d3m->num_triangles * 3 * sizeof(uint16_t), index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->aabbs_buffer = engine->renderer.gpu->memory_manager->GetBuffer(sizeof(vk::AabbPositionsKHR), aabbs_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->setIndexCount(d3m->num_triangles * 3);
        mesh->setVertexCount(d3m->num_triangles * 3);
        mesh->setVertexInputAttributeDescription(D3M::Vertex::getAttributeDescriptions());
        mesh->setVertexInputBindingDescription(D3M::Vertex::getBindingDescriptions());

        model->meshes.push_back(std::move(mesh));

        engine->worker_pool.addWork(std::make_unique<lotus::ParticleModelInitTask>(engine->renderer.getCurrentImage(), model, std::move(vertices), sizeof(D3M::Vertex), max_dist));
    }
}
