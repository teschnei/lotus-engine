#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace FFXI
{
    class MMB
    {
    public:
        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec3 color;
            glm::vec2 tex_coord;

            static std::vector<vk::VertexInputBindingDescription> getBindingDescriptions();
            static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions();
        };
        struct Model
        {
            char textureName[16];
            uint16_t blending;
            std::vector<Vertex> vertices;
            std::vector<uint16_t> indices;
            vk::PrimitiveTopology topology;
        };
        MMB(uint8_t* buffer, size_t max_len);

        static bool DecodeMMB(uint8_t* buffer, size_t max_len);

        char name[16];
        std::vector<Model> models;
    private:
        
    };
}
