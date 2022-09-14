#include "animation_component.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus::Component
{
    AnimationComponent::AnimationComponent(Entity* _entity, Engine* _engine, std::unique_ptr<Skeleton>&& _skeleton) :
        Component(_entity, _engine), skeleton(std::move(_skeleton))
    {
        skeleton_bone_buffer = engine->renderer->gpu->memory_manager->GetBuffer(sizeof(BufferBone) * skeleton->bones.size() * engine->renderer->getFrameCount(),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
        playAnimationLoop("idl");
    }

    Task<> AnimationComponent::tick(time_point time, duration delta)
    {
        if (current_animation)
        {
            duration animation_delta = time - animation_start;
            if (animation_delta < 0ms) animation_delta = 0ms;
            //all this just to floor the duration's rep and cast it back to a uint64_t
            auto frame_duration = duration(std::chrono::nanoseconds(static_cast<uint64_t>((current_animation->frame_duration / anim_speed).count())));
            if ((animation_delta / frame_duration) >= (current_animation->transforms.size() - 1) && next_anim)
            {
                /*
                for (uint32_t i = 0; i < skeleton->bones.size(); ++i)
                {
                    auto& bone = skeleton->bones[i];
                    auto transform = applyTransform(i, current_animation->transforms[current_animation->transforms.size() - 1][i]);
                    bone.rot = transform.rot;
                    bone.trans = transform.trans;
                    bone.scale = transform.scale;
                }
                */
                changeAnimation(*next_anim, 1.f);
                next_anim.reset();
                co_return;
            }
            if (animation_delta < interpolation_time)
            {
                float frame_f = static_cast<float>(animation_delta.count()) / static_cast<float>(interpolation_time.count());
                size_t frame = (interpolation_time / frame_duration) % current_animation->transforms.size();
                for (uint32_t i = 0; i < skeleton->bones.size(); ++i)
                {
                    if (const auto& transform = current_animation->transforms[frame].find(i); transform != current_animation->transforms[frame].end())
                    {
                        auto& bone = skeleton->bones[i];
                        std::tie(bone.rot, bone.trans, bone.scale) = interpBone(
                            lotus::Animation::BoneTransform{ .rot = bones_interpolate[i].rot, .trans = bones_interpolate[i].trans, .scale = bones_interpolate[i].scale },
                            applyTransform(i, transform->second), frame_f);
                    }
                }
            }
            else
            {
                float frame_f = static_cast<float>((animation_delta % frame_duration).count()) / static_cast<float>(frame_duration.count());
                size_t frame = (animation_delta / frame_duration) % current_animation->transforms.size();
                uint32_t next_frame = (frame + 1) % current_animation->transforms.size();
                for (uint32_t i = 0; i < skeleton->bones.size(); ++i)
                {
                    if (const auto& transform = current_animation->transforms[frame].find(i); transform != current_animation->transforms[frame].end())
                    {
                        if (const auto& next_transform = current_animation->transforms[next_frame].find(i); next_transform != current_animation->transforms[next_frame].end())
                        {
                            auto& bone = skeleton->bones[i];
                            std::tie(bone.rot, bone.trans, bone.scale) = interpBone(applyTransform(i, transform->second), applyTransform(i, next_transform->second), frame_f);
                        }
                    }
                }
            }
        }
        co_await renderWork();
    }

    WorkerTask<> AnimationComponent::renderWork()
    {
        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *engine->renderer->graphics_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        auto command_buffer = std::move(command_buffers[0]);

        command_buffer->begin(begin_info);

        auto skeleton = this->skeleton.get();
        auto staging_buffer = engine->renderer->gpu->memory_manager->GetBuffer(sizeof(AnimationComponent::BufferBone) * skeleton->bones.size(), vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        AnimationComponent::BufferBone* buffer = static_cast<AnimationComponent::BufferBone*>(staging_buffer->map(0, VK_WHOLE_SIZE, {}));
        for (size_t i = 0; i < skeleton->bones.size(); ++i)
        {
            buffer[i].trans = skeleton->bones[i].trans;
            buffer[i].scale = skeleton->bones[i].scale;
            buffer[i].rot.x = skeleton->bones[i].rot.x;
            buffer[i].rot.y = skeleton->bones[i].rot.y;
            buffer[i].rot.z = skeleton->bones[i].rot.z;
            buffer[i].rot.w = skeleton->bones[i].rot.w;
        }
        staging_buffer->unmap();

        vk::BufferCopy copy_region;
        copy_region.srcOffset = 0;
        copy_region.dstOffset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * engine->renderer->getCurrentFrame();
        copy_region.size = skeleton->bones.size() * sizeof(AnimationComponent::BufferBone);
        command_buffer->copyBuffer(staging_buffer->buffer, skeleton_bone_buffer->buffer, copy_region);

        vk::BufferMemoryBarrier2KHR barrier
        {
            .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
            .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = skeleton_bone_buffer->buffer,
            .offset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * engine->renderer->getCurrentFrame(),
            .size = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size()
        };

        command_buffer->pipelineBarrier2KHR({
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &barrier
        });
        command_buffer->end();

        engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(staging_buffer), std::move(command_buffer));
        co_return;
    }

    void AnimationComponent::playAnimation(std::string name, float speed, std::optional<std::string> _next_anim)
    {
        loop = false;
        next_anim = _next_anim;
        changeAnimation(name, speed);
    }

    void AnimationComponent::playAnimation(std::string name, duration anim_duration, std::optional<std::string> _next_anim)
    {
        auto& animation = skeleton->animations[name];
        auto total_duration = animation->frame_duration * (animation->transforms.size() - 1);
        auto speed = (float)total_duration.count() / anim_duration.count();
        playAnimation(name, speed, _next_anim);
    }

    void AnimationComponent::playAnimationLoop(std::string name, float speed, uint8_t _repetitions)
    {
        loop = true;
        repetitions = _repetitions;
        changeAnimation(name, speed);
    }

    void AnimationComponent::playAnimationLoop(std::string name, duration anim_duration, uint8_t _repetitions)
    {
        auto& animation = skeleton->animations[name];
        auto total_duration = animation->frame_duration * (animation->transforms.size() - 1);
        auto speed = (float)total_duration.count() / anim_duration.count();
        playAnimationLoop(name, speed, _repetitions);
    }

    void AnimationComponent::changeAnimation(std::string name, float speed)
    {
        auto new_anim = skeleton->animations[name];
        if (speed != anim_speed)
            anim_speed = speed;
        if (new_anim != current_animation)
        {
            current_animation = new_anim;
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

    lotus::Animation::BoneTransform AnimationComponent::applyTransform(size_t bone_index, const lotus::Animation::BoneTransform& transform)
    {
        auto& bone = skeleton->bone_data.bones[bone_index];
        if (bone_index == 0)
        {
            return { transform.rot * bone.rot, bone.trans + transform.trans, transform.scale };
        }
        else
        {
            auto& parent_bone = skeleton->bones[bone.parent_bone];
            lotus::Animation::BoneTransform local_transform = { transform.rot * bone.rot, bone.trans + transform.trans, transform.scale };

            return { parent_bone.rot * local_transform.rot, parent_bone.trans + (parent_bone.rot * local_transform.trans), local_transform.scale * parent_bone.scale };
        }
    }

    std::tuple<glm::quat, glm::vec3, glm::vec3> AnimationComponent::interpBone(lotus::Animation::BoneTransform b1, lotus::Animation::BoneTransform b2, float interp)
    {
        return { glm::slerp(b1.rot, b2.rot, interp), glm::mix(b1.trans, b2.trans, interp), glm::mix(b1.scale, b2.scale, interp) };
    }
}
