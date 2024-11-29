module;

#include <chrono>
#include <coroutine>
#include <memory>
#include <optional>

export module lotus:entity.component.animation;

import :core.engine;
import :entity.component;
import :renderer.animation;
import :renderer.memory;
import :renderer.skeleton;
import :renderer.vulkan.renderer;
import :util;
import glm;
import vulkan_hpp;

export namespace lotus::Component
{
class AnimationComponent : public Component<AnimationComponent>
{
public:
    explicit AnimationComponent(Entity*, Engine* engine, std::unique_ptr<Skeleton>&&);

    Task<> tick(time_point time, duration delta);
    void playAnimation(std::string name, float speed = 1.f, std::optional<std::string> next_anim = {});
    void playAnimation(std::string name, duration anim_duration, std::optional<std::string> next_anim = {});
    void playAnimationLoop(std::string name, float speed = 1.f, uint8_t repetitions = 0);
    void playAnimationLoop(std::string name, duration anim_duration, uint8_t repetitions = 0);

    std::unique_ptr<Skeleton> skeleton;
    struct BufferBone
    {
        glm::vec4 rot;
        glm::vec3 trans;
        float _pad1;
        glm::vec3 scale;
        float _pad2;
    };
    std::unique_ptr<Buffer> skeleton_bone_buffer;

protected:
    WorkerTask<> renderWork();
    void changeAnimation(std::string name, float speed);
    static constexpr duration interpolation_time{100ms};

    Animation* current_animation{nullptr};
    time_point animation_start;
    std::optional<std::string> next_anim;
    std::vector<Skeleton::Bone> bones_interpolate;
    float anim_speed{1.f};
    bool loop{true};
    uint8_t repetitions{0};
};

AnimationComponent::AnimationComponent(Entity* _entity, Engine* _engine, std::unique_ptr<Skeleton>&& _skeleton)
    : Component(_entity, _engine), skeleton(std::move(_skeleton))
{
    skeleton_bone_buffer = engine->renderer->gpu->memory_manager->GetBuffer(sizeof(BufferBone) * skeleton->bones.size() * engine->renderer->getFrameCount(),
                                                                            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                                                            vk::MemoryPropertyFlagBits::eDeviceLocal);
    playAnimationLoop("idl");
}

Task<> AnimationComponent::tick(time_point time, duration delta)
{
    if (current_animation)
    {
        duration animation_delta = time - animation_start;
        if (animation_delta < 0ms)
            animation_delta = 0ms;
        // all this just to floor the duration's rep and cast it back to a uint64_t
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
    auto staging_buffer =
        engine->renderer->gpu->memory_manager->GetBuffer(sizeof(AnimationComponent::BufferBone) * skeleton->bones.size(), vk::BufferUsageFlagBits::eTransferSrc,
                                                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    AnimationComponent::BufferBone* buffer = static_cast<AnimationComponent::BufferBone*>(staging_buffer->map(0, vk::WholeSize, {}));
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

    vk::BufferMemoryBarrier2KHR barrier{.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                                        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                        .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                                        .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                                        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                                        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                                        .buffer = skeleton_bone_buffer->buffer,
                                        .offset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * engine->renderer->getCurrentFrame(),
                                        .size = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size()};

    command_buffer->pipelineBarrier2KHR({.bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &barrier});
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
        // copy current bones so that we can interpolate off them to the new animation
        // bones_interpolate = skeleton->bones;
        bones_interpolate.clear();
        for (const auto& bone : skeleton->bones)
        {
            bones_interpolate.emplace_back(bone);
        }
    }
}
} // namespace lotus::Component
