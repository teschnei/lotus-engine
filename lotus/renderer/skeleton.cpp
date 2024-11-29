module;

#include <memory>

module lotus;

import :renderer.skeleton;
import glm;

namespace lotus
{
Skeleton::Skeleton(const BoneData& bone_data)
{
    for (const auto& bone : bone_data.bones)
    {
        Bone new_bone = bone;

        if (!bones.empty())
        {
            const Bone& parent = bone_data.bones[bone.parent_bone];
            new_bone.rot = parent.rot * bone.rot;
            new_bone.trans = parent.trans + (parent.rot * bone.trans);
        }
        bones.push_back(std::move(new_bone));
    }
}

void Skeleton::BoneData::addBone(uint8_t parent_bone, glm::quat rot, glm::vec3 trans) { bones.emplace_back(parent_bone, rot, trans); }
} // namespace lotus
