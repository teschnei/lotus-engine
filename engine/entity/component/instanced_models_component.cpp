#include "instanced_models_component.h"
#include "engine/core.h"

namespace lotus::Component
{
    InstancedModelsComponent::InstancedModelsComponent(Entity* _entity, Engine* _engine, std::vector<std::shared_ptr<Model>> _models,
        const std::vector<InstanceInfo>& _instances, std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> _instance_offsets) :
        Component(_entity, _engine), models(_models), instances(_instances), instance_offsets(_instance_offsets)
    {
    }

    WorkerTask<> InstancedModelsComponent::init()
    {
        instance_buffer = engine->renderer->gpu->memory_manager->GetBuffer(sizeof(InstanceInfo) * instances.size(),
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::CommandBufferAllocateInfo alloc_info;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
            .commandPool = *engine->renderer->graphics_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        });
        auto command_buffer = std::move(command_buffers[0]);

        command_buffer->begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });

        vk::DeviceSize buffer_size = sizeof(InstanceInfo) * instances.size();

        auto staging_buffer = engine->renderer->gpu->memory_manager->GetBuffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void* data = staging_buffer->map(0, buffer_size, {});
        memcpy(data, instances.data(), buffer_size);
        staging_buffer->unmap();

        vk::BufferCopy copy_region {
            .size = buffer_size
        };
        command_buffer->copyBuffer(staging_buffer->buffer, instance_buffer->buffer, copy_region);

        command_buffer->end();
        engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(command_buffer), std::move(staging_buffer));

        co_return;
    }

    Task<> InstancedModelsComponent::tick(time_point time, duration delta)
    {
        co_return;
    }

    std::vector<std::shared_ptr<Model>> InstancedModelsComponent::getModels() const
    {
        return models;
    }

    vk::Buffer InstancedModelsComponent::getInstanceBuffer() const
    {
        return instance_buffer->buffer;
    }

    std::pair<vk::DeviceSize, uint32_t> InstancedModelsComponent::getInstanceOffset(const std::string& name) const
    {
        auto o = instance_offsets.find(name);
        if (o != instance_offsets.end())
            return o->second;
        return { 0,0 };
    }

    InstancedModelsComponent::InstanceInfo InstancedModelsComponent::getInstanceInfo(vk::DeviceSize size) const
    {
        return instances[size];
    }
}
