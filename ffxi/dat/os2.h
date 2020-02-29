#pragma once

#include "dat_chunk.h"
#include <vector>
#include <glm/glm.hpp>
#include <string>
#include <engine/renderer/vulkan/vulkan_inc.h>

namespace FFXI
{
    class OS2 : public DatChunk
    {
    public:

        struct WeightingVertex
        {
            glm::vec3 pos;
            float _pad1;
            glm::vec3 norm;
            float weight;
            uint32_t bone_index;
            uint32_t mirror_axis;
            glm::vec2 uv;
        };

        struct WeightingVertexMirror
        {
            glm::vec3 pos;
            glm::vec3 norm;
            float weight;
            uint8_t bone_index;
            uint8_t bone_index_mirror;
            uint8_t mirror_axis;
        };

        struct Vertex
        {
            glm::vec3 pos;
            float _pad;
            glm::vec3 norm;
            float _pad2;
            glm::vec2 uv;
            glm::vec2 _pad3;

            static std::vector<vk::VertexInputBindingDescription> getBindingDescriptions();
            static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions();
        };

        struct Mesh
        {
            std::vector<std::pair<uint16_t, glm::vec2>> indices;
            std::string tex_name;
            float specular_exponent {};
            float specular_intensity {};
        };

        OS2(char* _name, uint8_t* _buffer, size_t _len);
        std::vector<Mesh> meshes;
        std::vector<std::pair<WeightingVertexMirror, WeightingVertexMirror>> vertices;
        bool mirror;
    };
}
