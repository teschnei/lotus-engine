#include "animation_component.h"
#include "engine/core.h"
#include "engine/entity/deformable_entity.h"
#include "engine/renderer/skeleton.h"

namespace lotus
{
    AnimationComponent::AnimationComponent(Entity* _entity, Engine* _engine, std::unique_ptr<Skeleton>&& _skeleton, size_t _vertex_stride) : Component(_entity, _engine), skeleton(std::move(_skeleton)), vertex_stride(_vertex_stride)
    {
        skeleton_bone_buffer = engine->renderer->gpu->memory_manager->GetBuffer(sizeof(BufferBone) * skeleton->bones.size() * engine->renderer->getImageCount(), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

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

    Task<> AnimationComponent::render(Engine* engine, std::shared_ptr<Entity> sp)
    {
        co_await renderWork();
    }

    WorkerTask<> AnimationComponent::renderWork()
    {
        auto entity = static_cast<DeformableEntity*>(this->entity);
        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *engine->renderer->compute_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        auto command_buffer = std::move(command_buffers[0]);

        command_buffer->begin(begin_info);

        auto anim_component = entity->animation_component;
        auto skeleton = anim_component->skeleton.get();
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
        copy_region.dstOffset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * engine->renderer->getCurrentImage();
        copy_region.size = skeleton->bones.size() * sizeof(AnimationComponent::BufferBone);
        command_buffer->copyBuffer(staging_buffer->buffer, anim_component->skeleton_bone_buffer->buffer, copy_region);

        vk::BufferMemoryBarrier barrier;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = anim_component->skeleton_bone_buffer->buffer;
        barrier.offset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * engine->renderer->getCurrentImage();
        barrier.size = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size();
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, nullptr, barrier, nullptr);
        command_buffer->end();

        engine->worker_pool->command_buffers.compute.queue(*command_buffer);
        engine->worker_pool->frameQueue(std::move(staging_buffer), std::move(command_buffer));
        co_return;
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
