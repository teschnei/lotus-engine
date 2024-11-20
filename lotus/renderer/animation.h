#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map>
#include "lotus/types.h"

namespace lotus
{
    class Animation
    {
    public:

        struct BoneTransform
        {
            glm::quat rot;
            glm::vec3 trans;
            glm::vec3 scale;
        };

        Animation(std::string name, duration frame_duration);
        std::string name;
        duration frame_duration;

        void addFrameData(uint32_t frame, uint32_t bone_index, uint32_t parent_bone_index, glm::quat rot, glm::vec3 trans, BoneTransform transform);

        std::vector<std::map<uint32_t, BoneTransform>> transforms;
    };
}
