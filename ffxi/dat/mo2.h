#pragma once

#include "engine/types.h"
#include <string>
#include <vector>
#include <map>
#include <glm/gtc/quaternion.hpp>

namespace FFXI
{
    class MO2
    {
    public:

        struct Frame
        {
            glm::quat rot;
            glm::vec3 trans;
            glm::vec3 scale;
        };

        MO2(uint8_t* buffer, size_t max_len, char name[4]);

        std::string name;
        uint32_t frames;
        float speed;
        std::map<uint32_t, std::vector<Frame>> animation_data;
    };
}
