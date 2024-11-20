#include "material.h"
#include "lotus/core.h"
#include "lotus/renderer/memory.h"
#include "lotus/renderer/vulkan/renderer.h"

namespace lotus
{
    struct MaterialBufferBlock
    {
        uint32_t texture_index;
        float specular_exponent;
        float specular_intensity;
        uint32_t light_type;
    };

    size_t Material::getMaterialBufferSize(Engine* engine)
    {
        return engine->renderer->uniform_buffer_align_up(sizeof(MaterialBufferBlock));
    }

    Material::Material(std::shared_ptr<Buffer> _buffer, uint32_t _buffer_offset, std::shared_ptr<Texture> _texture, uint32_t _light_type, float _specular_exponent, float _specular_intensity)
        : buffer(_buffer), buffer_offset(_buffer_offset), specular_exponent(_specular_exponent), specular_intensity(_specular_intensity), light_type(_light_type), texture(_texture)
    {
    }

    WorkerTask<std::shared_ptr<Material>> Material::make_material(Engine* engine, std::shared_ptr<Buffer> buffer, uint32_t buffer_offset,
        std::shared_ptr<Texture> texture, uint32_t light_type, float specular_exponent, float specular_intensity)
    {
        std::shared_ptr<Material> material = std::shared_ptr<Material>(new Material(buffer, buffer_offset, texture, light_type, specular_exponent, specular_intensity));

        auto buffer_size = engine->renderer->uniform_buffer_align_up(sizeof(MaterialBufferBlock));

        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *engine->renderer->compute_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

        auto staging_buffer = engine->renderer->gpu->memory_manager->GetBuffer(buffer_size,
            vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        MaterialBufferBlock* mapped = static_cast<MaterialBufferBlock*>(staging_buffer->map(0, buffer_size, {}));
        mapped->texture_index = material->texture->getDescriptorIndex();
        mapped->specular_exponent = material->specular_exponent;
        mapped->specular_intensity = material->specular_intensity;
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
}
