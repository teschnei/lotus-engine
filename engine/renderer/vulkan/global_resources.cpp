#include "global_resources.h"

#include "engine/core.h"
#include "engine/renderer/model.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    GlobalResources::GlobalResources(Engine* _engine, Renderer* renderer) : engine(_engine)
    {
        mesh_info_buffer = renderer->gpu->memory_manager->GetBuffer(max_resource_index * sizeof(MeshInfo) * renderer->getFrameCount(), vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostVisible);
        mesh_info_buffer_mapped = (MeshInfo*)mesh_info_buffer->map(0, max_resource_index * sizeof(MeshInfo) * renderer->getFrameCount(), {});
    }

    GlobalResources::~GlobalResources()
    {
        if (mesh_info_buffer_mapped)
            mesh_info_buffer->unmap();
    }

    void GlobalResources::BindResources(uint32_t image)
    {
        if (engine->renderer->raytracer)
        {
            auto descriptor_vertex_info_count = descriptor_vertex_count.load();
            auto descriptor_vertex_prev_info_count = descriptor_vertex_prev_count.load();
            auto descriptor_index_info_count = descriptor_index_count.load();
            auto descriptor_material_texture_info_count = descriptor_material_texture_count.load();
            auto offset = static_binding_offset.load();

            vk::DescriptorBufferInfo mesh_info;
            mesh_info.buffer = mesh_info_buffer->buffer;
            mesh_info.offset = sizeof(MeshInfo) * max_resource_index * image;
            mesh_info.range = sizeof(MeshInfo) * max_resource_index;

            auto descriptor_set = engine->renderer->raytracer->getResourceDescriptorSet(image);

            std::array writes
            {
                vk::WriteDescriptorSet { //vertex
                    .dstSet = descriptor_set,
                    .dstBinding = 1,
                    .dstArrayElement = offset,
                    .descriptorCount = descriptor_vertex_info_count,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = descriptor_vertex_info.data()
                },
                vk::WriteDescriptorSet { //vertex prev
                    .dstSet = descriptor_set,
                    .dstBinding = 2,
                    .dstArrayElement = 0,
                    .descriptorCount = descriptor_vertex_prev_info_count,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = descriptor_vertex_prev_info.data(),
                },
                vk::WriteDescriptorSet { //index
                    .dstSet = descriptor_set,
                    .dstBinding = 3,
                    .dstArrayElement = offset,
                    .descriptorCount = descriptor_index_info_count,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = descriptor_index_info.data(),
                },
                vk::WriteDescriptorSet { //texture
                    .dstSet = descriptor_set,
                    .dstBinding = 4,
                    .dstArrayElement = offset,
                    .descriptorCount = descriptor_material_texture_info_count,
                    .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                    .pImageInfo = descriptor_texture_info.data(),
                },
                vk::WriteDescriptorSet { //material
                    .dstSet = descriptor_set,
                    .dstBinding = 5,
                    .dstArrayElement = offset,
                    .descriptorCount = descriptor_material_texture_info_count,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo = descriptor_material_info.data(),
                },
                vk::WriteDescriptorSet { //mesh info
                    .dstSet = descriptor_set,
                    .dstBinding = 6,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &mesh_info
                },
            };

            engine->renderer->gpu->device->updateDescriptorSets(writes, nullptr);
        }
        mesh_info_buffer->flush(max_resource_index * sizeof(MeshInfo) * image, max_resource_index * sizeof(MeshInfo));
    }

    void GlobalResources::Reset()
    {
        descriptor_vertex_count = 0;
        descriptor_vertex_prev_count = 0;
        descriptor_index_count = 0;
        descriptor_material_texture_count = 0;
        mesh_info_offset = 0;
    }

    uint16_t GlobalResources::pushVertexInfo(std::span<vk::DescriptorBufferInfo> info)
    {
        auto index = descriptor_vertex_count.fetch_add(info.size());
        std::ranges::copy(info, descriptor_vertex_info.begin() + index);
        return index + static_binding_offset_data;
    }

    uint16_t GlobalResources::pushVertexPrevInfo(std::span<vk::DescriptorBufferInfo> info)
    {
        auto index = descriptor_vertex_prev_count.fetch_add(info.size());
        std::ranges::copy(info, descriptor_vertex_prev_info.begin() + index);
        return index;
    }
    
    uint16_t GlobalResources::pushIndexInfo(std::span<vk::DescriptorBufferInfo> info)
    {
        auto index = descriptor_index_count.fetch_add(info.size());
        std::ranges::copy(info, descriptor_index_info.begin() + index);
        return index + static_binding_offset_data;
    }

    uint16_t GlobalResources::pushMaterialTextureInfo(std::span<vk::DescriptorBufferInfo> info_material, std::span<vk::DescriptorImageInfo> info_texture)
    {
        auto index = descriptor_material_texture_count.fetch_add(std::max(info_material.size(), info_texture.size()));
        std::ranges::copy(info_material, descriptor_material_info.begin() + index);
        std::ranges::copy(info_texture, descriptor_texture_info.begin() + index);
        return index + static_binding_offset_data;
    }

    uint16_t GlobalResources::pushMeshInfo(std::span<MeshInfo> info)
    {
        auto index = mesh_info_offset.fetch_add(info.size());
        std::ranges::copy(info, mesh_info_buffer_mapped + (engine->renderer->getCurrentFrame() * max_resource_index) + index + static_binding_offset_data);
        return index + static_binding_offset_data;
    }

    GlobalResources::MeshInfo& GlobalResources::getMeshInfo(uint16_t index)
    {
        return mesh_info_buffer_mapped[index + max_resource_index * engine->renderer->getCurrentFrame()];
    }
}
