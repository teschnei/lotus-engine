#pragma once

#include "dat_chunk.h"
#include "engine/renderer/model.h"
#include <glm/glm.hpp>

namespace FFXI
{
    class D3M : public DatChunk
    {
    public:
        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 normal;
            uint32_t color;
            glm::vec2 uv;

            static std::vector<vk::VertexInputBindingDescription> getBindingDescriptions();
            static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions();
        };
        D3M(char* name, uint8_t* buffer, size_t len);

        std::string texture_name;
        uint16_t num_triangles{ 0 };
        Vertex* vertex_buffer{ nullptr };
    };

    class D3MLoader : public lotus::ModelLoader
    {
    public:
        D3MLoader(D3M* _d3m) : d3m(_d3m) {}
        virtual void LoadModel(std::shared_ptr<lotus::Model>&) override;
    private:
        D3M* d3m;
    };
}

