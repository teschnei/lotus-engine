#include "instanced_models_component.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus::Component
{
    InstancedModelsComponent::InstancedModelsComponent(Entity* _entity, Engine* _engine, std::vector<std::shared_ptr<Model>> _models,
        const std::vector<InstanceInfo>& _instances, std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> _instance_offsets) :
        Component(_entity, _engine), instances(_instances), instance_offsets(_instance_offsets)
    {
        for (const auto& model : _models)
        {
            models.push_back({
                .model = model,
                .mesh_infos = engine->renderer->global_descriptors->getMeshInfoBuffer(model->meshes.size())
            });
        }
    }

    WorkerTask<> InstancedModelsComponent::init()
    {
        instance_buffer = engine->renderer->gpu->memory_manager->GetBuffer(sizeof(InstanceInfo) * instances.size(),
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::CommandBufferAllocateInfo alloc_info;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
            .commandPool = *engine->renderer->compute_pool,
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
        auto compute = engine->renderer->async_compute->compute(std::move(command_buffer));

        for (const auto& model : models)
        {
            for (size_t i = 0; i < model.model->meshes.size(); ++i)
            {
                const auto& mesh = model.model->meshes[i];
                model.mesh_infos->buffer_view[i] = {
                    .vertex_offset = mesh->vertex_descriptor_index->index,
                    .index_offset = mesh->index_descriptor_index->index,
                    .indices = static_cast<uint32_t>(mesh->getIndexCount()),
                    .material_index = mesh->material->getIndex(),
                    .scale = glm::vec3{1.0},
                    .billboard = 0,
                    .colour = glm::vec4{1.0},
                    .uv_offset = glm::vec2{0.0},
                    .animation_frame = 0,
                    .vertex_prev_offset = mesh->vertex_descriptor_index->index,
                    .model_prev = glm::mat4{1.0}
                };
            }
        }

        co_await compute;

        co_return;
    }

    std::span<const InstancedModelsComponent::ModelInfo> InstancedModelsComponent::getModels() const
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
