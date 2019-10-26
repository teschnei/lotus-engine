#include "animation_component.h"
#include "engine/core.h"
#include "engine/entity/renderable_entity.h"
#include "engine/renderer/skeleton.h"
#include "engine/task/skin_mesh.h"

namespace lotus
{
    AnimationComponent::AnimationComponent(Entity* _entity, Engine* _engine, std::unique_ptr<Skeleton>&& _skeleton, size_t _vertex_stride) : Component(_entity), skeleton(std::move(_skeleton)), vertex_stride(_vertex_stride), engine(_engine)
    {
        playAnimation("idl0");
        for (size_t i = 0; i < skeleton->bones.size(); ++i)
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
            //engine->worker_pool.addWork(std::make_unique<SkinMeshTask>(static_cast<RenderableEntity*>(entity)));
            duration animation_delta = time - animation_start;
            int frame = (animation_delta / current_animation->frame_duration) % current_animation->transforms.size();
            int next_frame = (frame + 1) % current_animation->transforms.size();
            for (size_t i = 0; i < skeleton->bones.size(); ++i)
            {
                auto& bone = skeleton->bones[i];
                bone.rot = current_animation->transforms[frame][i].rot;
                bone.trans = current_animation->transforms[frame][i].trans;
                bone.scale = current_animation->transforms[frame][i].scale;
            }
        }
    }

    void AnimationComponent::playAnimation(std::string name)
    {
        current_animation = skeleton->animations[name].get();
        animation_start = sim_clock::now();
    }
}
