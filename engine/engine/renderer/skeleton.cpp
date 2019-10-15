#include "skeleton.h"

namespace lotus
{
    void Skeleton::addBone(uint8_t parent_bone, glm::quat rot, glm::vec3 trans)
    {
        Bone bone{ parent_bone, rot, rot * trans };

        if (!bones.empty())
        {
            Bone& parent = bones[parent_bone];
            bone.rot = parent.rot * rot;
            bone.trans = parent.trans + (parent.rot * trans);
        }
        else
        {
            bone.rot = bone.local_rot;
            bone.trans = bone.local_trans;
        }
        bones.push_back(std::move(bone));
    }

}