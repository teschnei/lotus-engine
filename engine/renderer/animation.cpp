#include "animation.h"
#include "skeleton.h"

namespace lotus
{
    Animation::Animation(std::string _name, duration _frame_duration) : name(_name), frame_duration(_frame_duration)
    {
    }

    void Animation::addFrameData(uint32_t frame, uint32_t bone_index, uint32_t parent_bone_index, glm::quat rot, glm::vec3 trans, BoneTransform transform)
    {
        //multiply with skeleton
        if (frame >= transforms.size())
        {
            transforms.resize(frame+1);
        }

        BoneTransform new_transform;

        if (bone_index != 0)
        {
            BoneTransform local_transform = { transform.rot * rot, trans + transform.trans,  transform.scale };
            const BoneTransform& parent_transform = transforms[frame][parent_bone_index];
            new_transform = { parent_transform.rot * local_transform.rot, parent_transform.trans + (parent_transform.rot * local_transform.trans), local_transform.scale * parent_transform.scale };
        }
        else
        {
            new_transform = { transform.rot * rot, trans + transform.trans, transform.scale };
        }

        transforms[frame][bone_index] = new_transform;
    }
}
