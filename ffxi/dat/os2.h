#pragma once

#include "engine/types.h"
#include <vector>
#include <glm/glm.hpp>
#include <string>
#include <vulkan/vulkan.hpp>

namespace FFXI
{
    class OS2
    {
    public:

        struct WeightingVertex
        {
            glm::vec3 pos;
            glm::vec3 norm;
            float weight;
            uint8_t bone_index;
            uint8_t mirror_axis;
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
            glm::vec3 norm;
            glm::vec2 uv;

            static std::vector<vk::VertexInputBindingDescription> getBindingDescriptions();
            static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions();
        };

        struct Mesh
        {
            std::vector<std::pair<uint16_t, glm::vec2>> indices;
            std::string tex_name;
        };

        OS2(uint8_t* buffer, size_t max_len);
        std::vector<Mesh> meshes;
        std::vector<std::pair<WeightingVertexMirror, WeightingVertexMirror>> vertices;
        bool mirror;
    };
}
