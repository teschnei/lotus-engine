#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <glm/gtc/quaternion.hpp>
#include "engine/renderer/animation.h"

namespace lotus
{
    class Skeleton
    {
    public:
        class Bone
        {
        public:
            Bone(uint8_t _parent_bone, glm::quat _rot, glm::vec3 _trans, glm::vec3 scale = glm::vec3{1.f}) :
                parent_bone(_parent_bone), rot(_rot), trans(_trans) {}

            const uint8_t parent_bone;
            glm::quat rot{};
            glm::vec3 trans{};
            glm::vec3 scale{ 1.f };

            friend class Skeleton;
        };

        struct BoneData
        {
            std::vector<Bone> bones;
            void addBone(uint8_t parent_bone, glm::quat rot, glm::vec3 trans);
        };

        explicit Skeleton(const BoneData& bone_data);

        const BoneData& bone_data;
        std::vector<Bone> bones;
        std::unordered_map<std::string, Animation*> animations;
    };
}
