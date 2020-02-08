#pragma once

#include "dat_chunk.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

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
#pragma pack(pop)

        SK2(char* _name, uint8_t* _buffer, size_t _len);
        std::vector<Bone> bones;
    };
}
