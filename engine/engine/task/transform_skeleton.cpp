#include "transform_skeleton.h"
#include "engine/core.h"
#include "engine/worker_thread.h"
#include "engine/entity/component/animation_component.h"

namespace lotus
{
    TransformSkeletonTask::TransformSkeletonTask(RenderableEntity* _entity) : entity(_entity)
    {
    }

    void TransformSkeletonTask::Process(WorkerThread* thread)
    {
        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *thread->compute.command_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);
        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        command_buffer = std::move(command_buffers[0]);

        command_buffer->begin(begin_info, thread->engine->renderer.dispatch);

        auto anim_component = entity->animation_component;
        auto skeleton = anim_component->skeleton.get();
        staging_buffer = thread->engine->renderer.memory_manager->GetBuffer(sizeof(AnimationComponent::BufferBone) * skeleton->bones.size(), vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        AnimationComponent::BufferBone* buffer = static_cast<AnimationComponent::BufferBone*>(thread->engine->renderer.device->mapMemory(staging_buffer->memory,
            staging_buffer->memory_offset + skeleton->bones.size() * sizeof(AnimationComponent::BufferBone) * thread->engine->renderer.getCurrentImage(),
            skeleton->bones.size() * sizeof(AnimationComponent::BufferBone), {}, thread->engine->renderer.dispatch));
        for (size_t i = 0; i < skeleton->bones.size(); ++i)
        {
            buffer[i].trans = skeleton->bones[i].trans;
            buffer[i].scale = skeleton->bones[i].scale;
            buffer[i].rot.x = skeleton->bones[i].rot.x;
            buffer[i].rot.y = skeleton->bones[i].rot.y;
            buffer[i].rot.z = skeleton->bones[i].rot.z;
            buffer[i].rot.w = skeleton->bones[i].rot.w;
        }
        thread->engine->renderer.device->unmapMemory(staging_buffer->memory);

        vk::BufferCopy copy_region;
        copy_region.srcOffset = 0;
        copy_region.dstOffset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * thread->engine->renderer.getCurrentImage();
        copy_region.size = skeleton->bones.size() * sizeof(AnimationComponent::BufferBone);
        command_buffer->copyBuffer(staging_buffer->buffer, anim_component->skeleton_bone_buffer->buffer, copy_region, thread->engine->renderer.dispatch);
        command_buffer->end(thread->engine->renderer.dispatch);

        thread->compute.primary_buffers[thread->engine->renderer.getCurrentImage()].push_back(*command_buffer);
    }
}