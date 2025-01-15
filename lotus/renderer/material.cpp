module;

#include <coroutine>
#include <memory>

module lotus;

import :renderer.material;

import :core.engine;
import :renderer.memory;
import :renderer.vulkan.renderer;
import glm;
import vulkan_hpp;

namespace lotus
{
struct MaterialBufferBlock
{
    uint32_t texture_index;
    glm::vec2 roughness;
    float ior;
    uint32_t light_type;
};

size_t Material::getMaterialBufferSize(Engine* engine) { return engine->renderer->uniform_buffer_align_up(sizeof(MaterialBufferBlock)); }

Material::Material(std::shared_ptr<Buffer> _buffer, uint32_t _buffer_offset, std::shared_ptr<Texture> _texture, uint32_t _light_type, glm::vec2 _roughness,
                   float _ior)
    : buffer(_buffer), buffer_offset(_buffer_offset), roughness(_roughness), ior(_ior), light_type(_light_type), texture(_texture)
{
}

WorkerTask<std::shared_ptr<Material>> Material::make_material(Engine* engine, std::shared_ptr<Buffer> buffer, uint32_t buffer_offset,
                                                              std::shared_ptr<Texture> texture, uint32_t light_type, glm::vec2 roughness, float ior)
{
    std::shared_ptr<Material> material = std::shared_ptr<Material>(new Material(buffer, buffer_offset, texture, light_type, roughness, ior));

    auto buffer_size = engine->renderer->uniform_buffer_align_up(sizeof(MaterialBufferBlock));

    vk::CommandBufferAllocateInfo alloc_info = {};
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandPool = *engine->renderer->compute_pool;
    alloc_info.commandBufferCount = 1;

    auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

    auto staging_buffer = engine->renderer->gpu->memory_manager->GetBuffer(
        buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    MaterialBufferBlock* mapped = static_cast<MaterialBufferBlock*>(staging_buffer->map(0, buffer_size, {}));
    mapped->texture_index = material->texture->getDescriptorIndex();
    mapped->roughness = material->roughness;
    mapped->ior = material->ior;
    mapped->light_type = material->light_type;

    staging_buffer->unmap();

    vk::CommandBufferBeginInfo begin_info = {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    command_buffers[0]->begin(begin_info);

    vk::BufferCopy copy_region;
    copy_region.size = buffer_size;
    copy_region.dstOffset = material->buffer_offset;
    command_buffers[0]->copyBuffer(staging_buffer->buffer, material->buffer->buffer, copy_region);

    command_buffers[0]->end();

    co_await engine->renderer->async_compute->compute(std::move(command_buffers[0]));

    co_return material;
}
} // namespace lotus
