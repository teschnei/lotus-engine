#pragma once

#include "dat_chunk.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <array>
#include "engine/renderer/skeleton.h"

namespace FFXI
{
    class SK2 : public DatChunk
    {
    public:
#pragma pack(push,2)
        struct Bone
        {
            uint8_t parent_index;
            uint8_t _pad;
            glm::quat rot;
            glm::vec3 trans;
        };

        struct GeneratorPoint
        {
            uint8_t bone_index;
            uint8_t _pad;
            float unknown[3];
            glm::vec3 offset;
        };
#pragma pack(pop)

        static constexpr size_t GeneratorPointMax = 120;

        SK2(char* _name, uint8_t* _buffer, size_t _len);
        std::vector<Bone> bones;
        std::array<GeneratorPoint, GeneratorPointMax> generator_points;
    };
}
