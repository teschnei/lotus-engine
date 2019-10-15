#pragma once

#include "engine/types.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace FFXI
{
    class SK2
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

        SK2(uint8_t* buffer, size_t max_len);
        std::vector<Bone> bones;
    };
}
