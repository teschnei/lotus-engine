module;

#include <coroutine>
#include <cstring>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

export module lotus:entity.component.instanced_models;

import :core.engine;
import :entity.component;
import :renderer.memory;
import :renderer.model;
import :renderer.vulkan.renderer;
import :util;
import glm;
import vulkan_hpp;

export namespace lotus::Component
{
class InstancedModelsComponent : public Component<InstancedModelsComponent>
{
public:
    struct InstanceInfo
    {
        glm::mat4 model;
        glm::mat4 model_t;
        glm::mat3 model_it;
    };

    struct ModelInfo
    {
        std::shared_ptr<Model> model;
        std::unique_ptr<GlobalDescriptors::MeshInfoBuffer::View> mesh_infos;
    };

    explicit InstancedModelsComponent(Entity*, Engine* engine, std::vector<std::shared_ptr<Model>> models, const std::vector<InstanceInfo>& instances,
                                      const std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> instance_offsets);

    WorkerTask<> init();

    std::span<const ModelInfo> getModels() const;
    vk::Buffer getInstanceBuffer() const;
    std::pair<vk::DeviceSize, uint32_t> getInstanceOffset(const std::string& name) const;
    InstanceInfo getInstanceInfo(vk::DeviceSize offset) const;

protected:
    std::vector<ModelInfo> models;
    std::vector<InstanceInfo> instances;
    std::unique_ptr<Buffer> instance_buffer;
    std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> instance_offsets; // pair of offset/count
};

InstancedModelsComponent::InstancedModelsComponent(Entity* _entity, Engine* _engine, std::vector<std::shared_ptr<Model>> _models,
                                                   const std::vector<InstanceInfo>& _instances,
                                                   std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> _instance_offsets)
    : Component(_entity, _engine), instances(_instances), instance_offsets(_instance_offsets)
{
    for (const auto& model : _models)
    {
        models.push_back({.model = model, .mesh_infos = engine->renderer->global_descriptors->getMeshInfoBuffer(model->meshes.size())});
    }
}

WorkerTask<> InstancedModelsComponent::init()
{
    instance_buffer = engine->renderer->gpu->memory_manager->GetBuffer(sizeof(InstanceInfo) * instances.size(),
                                                                       vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                                                                       vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::CommandBufferAllocateInfo alloc_info;

    auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
        .commandPool = *engine->renderer->compute_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    });
    auto command_buffer = std::move(command_buffers[0]);

    command_buffer->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    vk::DeviceSize buffer_size = sizeof(InstanceInfo) * instances.size();

    auto staging_buffer = engine->renderer->gpu->memory_manager->GetBuffer(
        buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data = staging_buffer->map(0, buffer_size, {});
    memcpy(data, instances.data(), buffer_size);
    staging_buffer->unmap();

    vk::BufferCopy copy_region{.size = buffer_size};
    command_buffer->copyBuffer(staging_buffer->buffer, instance_buffer->buffer, copy_region);

    command_buffer->end();
    auto compute = engine->renderer->async_compute->compute(std::move(command_buffer));

    for (const auto& model : models)
    {
        for (size_t i = 0; i < model.model->meshes.size(); ++i)
        {
            const auto& mesh = model.model->meshes[i];
            auto material_buffer = mesh->material->getBuffer();
            model.mesh_infos->buffer_view[i] = {
                .vertex_buffer = engine->renderer->gpu->device->getBufferAddress({.buffer = model.model->vertex_buffer->buffer}) + mesh->vertex_offset,
                .vertex_prev_buffer = engine->renderer->gpu->device->getBufferAddress({.buffer = model.model->vertex_buffer->buffer}) + mesh->vertex_offset,
                .index_buffer = engine->renderer->gpu->device->getBufferAddress({.buffer = model.model->index_buffer->buffer}) + mesh->index_offset,
                .material = engine->renderer->gpu->device->getBufferAddress({.buffer = material_buffer.first}) + material_buffer.second,
                .scale = glm::vec3{1.0},
                .billboard = 0,
                .colour = glm::vec4{1.0},
                .uv_offset = glm::vec2{0.0},
                .animation_frame = 0,
                .index_count = static_cast<uint32_t>(mesh->getIndexCount()),
                .model_prev = glm::mat4{1.0},
            };
        }
    }

    co_await compute;

    co_return;
}

std::span<const InstancedModelsComponent::ModelInfo> InstancedModelsComponent::getModels() const { return models; }

vk::Buffer InstancedModelsComponent::getInstanceBuffer() const { return instance_buffer->buffer; }

std::pair<vk::DeviceSize, uint32_t> InstancedModelsComponent::getInstanceOffset(const std::string& name) const
{
    auto o = instance_offsets.find(name);
    if (o != instance_offsets.end())
        return o->second;
    return {0, 0};
}

InstancedModelsComponent::InstanceInfo InstancedModelsComponent::getInstanceInfo(vk::DeviceSize size) const { return instances[size]; }
} // namespace lotus::Component
