#include "animation.h"
#include "skeleton.h"

namespace lotus
{
    Animation::Animation(std::string _name, size_t frames, duration _frame_duration) : name(_name), frame_duration(_frame_duration)
    {
        transforms.resize(frames);
    }

    void Animation::addFrameData(uint32_t frame, uint32_t bone_index, uint32_t parent_bone_index, glm::quat rot, glm::vec3 trans, BoneTransform transform)
    {
        transforms[frame][bone_index] = transform;
    }
}
