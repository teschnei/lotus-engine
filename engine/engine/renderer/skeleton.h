#pragma once
#include <vector>
#include <glm/gtc/quaternion.hpp>

namespace lotus
{
    class Skeleton
    {
    public:
        class Bone
        {
        public:
            Bone(uint8_t _parent_bone, glm::quat _rot, glm::vec3 _trans) :
                parent_bone(_parent_bone), local_rot(_rot), local_trans(_trans) {}
        public:
            const uint8_t parent_bone;
            const glm::quat local_rot;
            const glm::vec3 local_trans;
            glm::quat rot {};
            glm::vec3 trans {};

            friend class Skeleton;
        };

        void addBone(uint8_t parent_bone, glm::quat rot, glm::vec3 trans);

        std::vector<Bone> bones;
    };
}
