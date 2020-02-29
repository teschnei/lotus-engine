#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map>
#include "engine/types.h"

namespace lotus
{
    class Skeleton;
    class Animation
    {
    public:

        struct BoneTransform
        {
            glm::quat rot;
            glm::vec3 trans;
            glm::vec3 scale;
        };

        Animation(Skeleton* skeleton);
        std::string name;
        Skeleton* skeleton;
        duration frame_duration;

        void addFrameData(uint32_t frame, uint32_t bone, BoneTransform transform);

        std::vector<std::map<uint32_t, BoneTransform>> transforms;
    };
}
