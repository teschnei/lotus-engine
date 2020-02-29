#include "animation.h"
#include "skeleton.h"

namespace lotus
{
    Animation::Animation(Skeleton* _skeleton) : skeleton(_skeleton)
    {
    }

    void Animation::addFrameData(uint32_t frame, uint32_t bone_index, BoneTransform transform)
    {
        //multiply with skeleton
        if (frame >= transforms.size())
        {
            transforms.resize(frame+1);
        }

        Skeleton::Bone& bone = skeleton->bones[bone_index];
        BoneTransform new_transform;

        if (bone_index != 0)
        {
            BoneTransform local_transform = { transform.rot * bone.local_rot, bone.local_trans + transform.trans,  transform.scale };
            const BoneTransform& parent_transform = transforms[frame][bone.parent_bone];
            new_transform = { parent_transform.rot * local_transform.rot, parent_transform.trans + (parent_transform.rot * local_transform.trans), local_transform.scale * parent_transform.scale };
        }
        else
        {
            new_transform = { transform.rot * bone.local_rot, bone.local_trans + transform.trans, transform.scale };
        }

        transforms[frame][bone_index] = new_transform;
    }

}
