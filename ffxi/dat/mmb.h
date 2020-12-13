#pragma once

#include <cstdint>
#include <vector>
#include <latch>
#include <atomic>
#include <glm/glm.hpp>
#include "dat_chunk.h"
#include "engine/renderer/vulkan/vulkan_inc.h"
#include "engine/renderer/model.h"

namespace FFXI
{
    class MMB : public DatChunk
    {
    public:
        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec3 color;
            glm::vec2 tex_coord;
            float _pad;
        };
        struct Mesh
        {
            char textureName[16];
            uint16_t blending;
            std::vector<Vertex> vertices;
            std::vector<uint16_t> indices;
            vk::PrimitiveTopology topology;
        };
        MMB(char* _name, uint8_t* _buffer, size_t _len, bool offset_vertices);

        static bool DecodeMMB(uint8_t* buffer, size_t max_len);

        char name[16];
        std::vector<Mesh> meshes;
    private:
        
    };

    class MMBLoader
    {
    public:
        static lotus::Task<> LoadModel(std::shared_ptr<lotus::Model>, lotus::Engine* engine, MMB* mmb);
    private:
        static void InitPipeline(lotus::Engine*);
        static inline vk::Pipeline pipeline;
        static inline vk::Pipeline pipeline_blend;
        static inline vk::Pipeline pipeline_shadowmap;
        static inline vk::Pipeline pipeline_shadowmap_blend;
        static inline std::latch pipeline_latch{ 1 };
        static inline std::atomic_flag pipeline_flag;
    };
}
