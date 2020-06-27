#include "animation_component.h"
#include "engine/core.h"
#include "engine/entity/deformable_entity.h"
#include "engine/renderer/skeleton.h"
#include "engine/task/transform_skeleton.h"

namespace lotus
{
    AnimationComponent::AnimationComponent(Entity* _entity, Engine* _engine, std::unique_ptr<Skeleton>&& _skeleton, size_t _vertex_stride) : Component(_entity, _engine), skeleton(std::move(_skeleton)), vertex_stride(_vertex_stride)
    {
        skeleton_bone_buffer = engine->renderer.gpu->memory_manager->GetBuffer(sizeof(BufferBone) * skeleton->bones.size() * engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

        //TODO: remove me
        playAnimation("idl0");
        for (uint32_t i = 0; i < skeleton->bones.size(); ++i)
        {
            auto& bone = skeleton->bones[i];
            bone.rot = skeleton->animations["idl0"]->transforms[0][i].rot;
            bone.trans = skeleton->animations["idl0"]->transforms[0][i].trans;
            bone.scale = skeleton->animations["idl0"]->transforms[0][i].scale;
        }
    }

    void AnimationComponent::tick(time_point time, duration delta)
    {
        if (current_animation)
        {
            duration animation_delta = time - animation_start;
            if (animation_delta < 0ms) animation_delta = 0ms;
            //all this just to floor the duration's rep and cast it back to a uint64_t
            auto frame_duration = duration(std::chrono::nanoseconds(static_cast<uint64_t>((current_animation->frame_duration / anim_speed).count())));
            if (animation_delta < interpolation_time)
            {
                float frame_f = static_cast<float>(animation_delta.count()) / static_cast<float>(interpolation_time.count());
                size_t frame = (interpolation_time / frame_duration) % current_animation->transforms.size();
                for (uint32_t i = 0; i < skeleton->bones.size(); ++i)
                {
                    auto& bone = skeleton->bones[i];
                    bone.rot = glm::slerp(bones_interpolate[i].rot, current_animation->transforms[frame][i].rot, frame_f);
                    bone.trans = glm::mix(bones_interpolate[i].trans, current_animation->transforms[frame][i].trans, frame_f);
                    bone.scale = glm::mix(bones_interpolate[i].scale, current_animation->transforms[frame][i].scale, frame_f);
                }
            }
            else
            {
                float frame_f = static_cast<float>((animation_delta % frame_duration).count()) / static_cast<float>(frame_duration.count());
                size_t frame = (animation_delta / frame_duration) % current_animation->transforms.size();
                uint32_t next_frame = (frame + 1) % current_animation->transforms.size();
                for (uint32_t i = 0; i < skeleton->bones.size(); ++i)
                {
                    auto& bone = skeleton->bones[i];
                    bone.rot = glm::slerp(current_animation->transforms[frame][i].rot, current_animation->transforms[next_frame][i].rot, frame_f);
                    bone.trans = glm::mix(current_animation->transforms[frame][i].trans, current_animation->transforms[next_frame][i].trans, frame_f);
                    bone.scale = glm::mix(current_animation->transforms[frame][i].scale, current_animation->transforms[next_frame][i].scale, frame_f);
                }
            }
        }
    }

    void AnimationComponent::render(Engine* engine, std::shared_ptr<Entity>& sp)
    {
        engine->worker_pool.addWork(std::make_unique<TransformSkeletonTask>(std::static_pointer_cast<DeformableEntity>(sp)));
    }

    void AnimationComponent::playAnimation(std::string name, float speed, std::optional<std::string> _next_anim)
    {
        loop = false;
        next_anim = _next_anim;
        changeAnimation(name, speed);
    }

    void AnimationComponent::playAnimationLoop(std::string name, float speed)
    {
        loop = true;
        changeAnimation(name, speed);
    }

    void AnimationComponent::changeAnimation(std::string name, float speed)
    {
        anim_speed = speed;
        current_animation = skeleton->animations[name].get();
        animation_start = sim_clock::now();
        //copy current bones so that we can interpolate off them to the new animation
        //bones_interpolate = skeleton->bones;
        bones_interpolate.clear();
        for (const auto& bone : skeleton->bones)
        {
            bones_interpolate.emplace_back(bone);
        }
    }
}
