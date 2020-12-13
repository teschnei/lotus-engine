#include "model.h"

#include <numeric>
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer_raytrace_base.h"

namespace lotus
{
    Model::Model(const std::string& _name) : name(_name)
    {
    }

    WorkerTask<> Model::InitWork(Engine* engine, std::vector<std::vector<uint8_t>>&& vertex_buffers, std::vector<std::vector<uint8_t>>&& index_buffers, uint32_t vertex_stride)
    {
        //priority: -1
        if (!vertex_buffers.empty())
        {
            std::vector<vk::AccelerationStructureGeometryKHR> raytrace_geometry;
            std::vector<vk::AccelerationStructureBuildOffsetInfoKHR> raytrace_offset_info;
            std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR> raytrace_create_info;
            vk::CommandBufferAllocateInfo alloc_info = {};
            alloc_info.level = vk::CommandBufferLevel::ePrimary;
            alloc_info.commandPool = *engine->renderer->graphics_pool;
            alloc_info.commandBufferCount = 1;

            auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

            vk::DeviceSize staging_buffer_size = 0;
            for (size_t i = 0; i < vertex_buffers.size(); ++i)
            {
                auto& vertex_buffer = vertex_buffers[i];
                auto& index_buffer = index_buffers[i];
                staging_buffer_size += vertex_buffer.size() + index_buffer.size();
            }

            auto staging_buffer = engine->renderer->gpu->memory_manager->GetBuffer(staging_buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            uint8_t* staging_buffer_data = static_cast<uint8_t*>(staging_buffer->map(0, staging_buffer_size, {}));

            vk::DeviceSize staging_buffer_offset = 0;

            auto command_buffer = std::move(command_buffers[0]);

            vk::CommandBufferBeginInfo begin_info = {};
            begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

            command_buffer->begin(begin_info);

            for (size_t i = 0; i < vertex_buffers.size(); ++i)
            {
                auto& vertex_buffer = vertex_buffers[i];
                auto& index_buffer = index_buffers[i];
                auto& mesh = meshes[i];

                memcpy(staging_buffer_data + staging_buffer_offset, vertex_buffer.data(), vertex_buffer.size());
                memcpy(staging_buffer_data + staging_buffer_offset + vertex_buffer.size(), index_buffer.data(), index_buffer.size());

                vk::BufferCopy copy_region;
                copy_region.srcOffset = staging_buffer_offset;
                copy_region.size = vertex_buffer.size();
                command_buffer->copyBuffer(staging_buffer->buffer, mesh->vertex_buffer->buffer, copy_region);
                copy_region.size = index_buffer.size();
                copy_region.srcOffset = vertex_buffer.size() + staging_buffer_offset;
                command_buffer->copyBuffer(staging_buffer->buffer, mesh->index_buffer->buffer, copy_region);

                if (engine->config->renderer.RaytraceEnabled() && !weighted)
                {
                    raytrace_geometry.emplace_back(vk::GeometryTypeKHR::eTriangles, vk::AccelerationStructureGeometryTrianglesDataKHR{
                        vk::Format::eR32G32B32Sfloat,
                        engine->renderer->gpu->device->getBufferAddressKHR(mesh->vertex_buffer->buffer),
                        vertex_stride,
                        vk::IndexType::eUint16,
                        engine->renderer->gpu->device->getBufferAddressKHR(mesh->index_buffer->buffer)
                        }, mesh->has_transparency ? vk::GeometryFlagsKHR{} : vk::GeometryFlagBitsKHR::eOpaque);

                    raytrace_offset_info.emplace_back(static_cast<uint32_t>((index_buffer.size() / sizeof(uint16_t))/3), 0, 0);

                    raytrace_create_info.emplace_back(vk::GeometryTypeKHR::eTriangles, static_cast<uint32_t>((index_buffer.size() / sizeof(uint16_t)) / 3),
                        vk::IndexType::eUint16, static_cast<uint32_t>(vertex_buffer.size() / vertex_stride), vk::Format::eR32G32B32Sfloat, false);
                }

                staging_buffer_offset += vertex_buffer.size() + index_buffer.size();
            }
            staging_buffer->unmap();

            if (engine->config->renderer.RaytraceEnabled() && !weighted)
            {
                RendererRaytraceBase* renderer = static_cast<RendererRaytraceBase*>(engine->renderer.get());
                //TODO: test if just buffermemorybarrier is faster
                vk::MemoryBarrier barrier;
                barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eAccelerationStructureReadKHR;
                command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, barrier, nullptr, nullptr);
                bottom_level_as = std::make_unique<BottomLevelAccelerationStructure>(renderer, *command_buffer, std::move(raytrace_geometry), std::move(raytrace_offset_info),
                    std::move(raytrace_create_info), false, lifetime == Lifetime::Long, BottomLevelAccelerationStructure::Performance::FastTrace);

                if (lifetime == Lifetime::Long && rendered)
                {
                    std::vector<vk::DescriptorBufferInfo> descriptor_vertex_info;
                    std::vector<vk::DescriptorBufferInfo> descriptor_index_info;
                    std::vector<vk::DescriptorBufferInfo> descriptor_material_info;
                    std::vector<vk::DescriptorImageInfo> descriptor_texture_info;
                    resource_index = renderer->resources->static_binding_offset.fetch_add(meshes.size());
                    for (size_t i = 0; i < meshes.size(); ++i)
                    {
                        auto& mesh = meshes[i];
                        descriptor_vertex_info.emplace_back(mesh->vertex_buffer->buffer, 0, VK_WHOLE_SIZE);
                        descriptor_index_info.emplace_back(mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE);
                        auto [buffer, offset] = mesh->material->getBuffer();
                        descriptor_material_info.emplace_back(buffer, offset, Material::getMaterialBufferSize(engine));
                        descriptor_texture_info.emplace_back(*mesh->material->texture->sampler, *mesh->material->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
                        mesh->material->index = resource_index + i;
                        for (size_t image = 0; image < engine->renderer->getImageCount(); ++image)
                        {
                            renderer->resources->mesh_info_buffer_mapped[image * GlobalResources::max_resource_index + resource_index + i] =
                            { resource_index + (uint32_t)i, resource_index + (uint32_t)i, (uint32_t)mesh->getIndexCount(), (uint32_t)mesh->material->index, glm::vec3{1.0}, 0, glm::vec4{1.f} };
                        }
                    }

                    vk::WriteDescriptorSet write_info_vertex;
                    write_info_vertex.descriptorType = vk::DescriptorType::eStorageBuffer;
                    write_info_vertex.dstArrayElement = resource_index;
                    write_info_vertex.dstBinding = 1;
                    write_info_vertex.descriptorCount = static_cast<uint32_t>(descriptor_vertex_info.size());
                    write_info_vertex.pBufferInfo = descriptor_vertex_info.data();

                    vk::WriteDescriptorSet write_info_index;
                    write_info_index.descriptorType = vk::DescriptorType::eStorageBuffer;
                    write_info_index.dstArrayElement = resource_index;
                    write_info_index.dstBinding = 2;
                    write_info_index.descriptorCount = static_cast<uint32_t>(descriptor_index_info.size());
                    write_info_index.pBufferInfo = descriptor_index_info.data();

                    vk::WriteDescriptorSet write_info_texture;
                    write_info_texture.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                    write_info_texture.dstArrayElement = resource_index;
                    write_info_texture.dstBinding = 3;
                    write_info_texture.descriptorCount = static_cast<uint32_t>(descriptor_texture_info.size());
                    write_info_texture.pImageInfo = descriptor_texture_info.data();

                    vk::WriteDescriptorSet write_info_material;
                    write_info_material.descriptorType = vk::DescriptorType::eUniformBuffer;
                    write_info_material.dstArrayElement = resource_index;
                    write_info_material.dstBinding = 4;
                    write_info_material.descriptorCount = static_cast<uint32_t>(descriptor_material_info.size());
                    write_info_material.pBufferInfo = descriptor_material_info.data();

                    std::vector<vk::WriteDescriptorSet> writes;
                    for (size_t i = 0; i < engine->renderer->getImageCount(); ++i)
                    {
                        write_info_vertex.dstSet = *renderer->rtx_descriptor_sets_const[i];
                        write_info_index.dstSet = *renderer->rtx_descriptor_sets_const[i];
                        write_info_texture.dstSet = *renderer->rtx_descriptor_sets_const[i];
                        write_info_material.dstSet = *renderer->rtx_descriptor_sets_const[i];
                        writes.push_back(write_info_vertex);
                        writes.push_back(write_info_index);
                        writes.push_back(write_info_texture);
                        writes.push_back(write_info_material);
                    }
                    {
                        std::lock_guard lk{ engine->renderer->resources->resource_descriptor_mutex };
                        engine->renderer->gpu->device->updateDescriptorSets(writes, nullptr);
                    }
                }
            }
            command_buffer->end();

            engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
            engine->worker_pool->gpuResource(std::move(staging_buffer), std::move(command_buffer));
        }
        co_return;
    }

    WorkerTask<> Model::InitWork(Engine* engine, std::vector<uint8_t>&& vertex_buffer, uint32_t vertex_stride, float aabb_dist)
    {
        if (!vertex_buffer.empty())
        {
            std::vector<vk::AccelerationStructureGeometryKHR> raytrace_geometry;
            std::vector<vk::AccelerationStructureBuildOffsetInfoKHR> raytrace_offset_info;
            std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR> raytrace_create_info;
            vk::CommandBufferAllocateInfo alloc_info = {};
            alloc_info.level = vk::CommandBufferLevel::ePrimary;
            alloc_info.commandPool = *engine->renderer->graphics_pool;
            alloc_info.commandBufferCount = 1;

            auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

            //assumes only 1 mesh
            auto& mesh = meshes[0];

            auto index_buffer = std::vector<uint16_t>(mesh->getIndexCount());
            std::iota(index_buffer.begin(), index_buffer.end(), 0);

            vk::DeviceSize staging_buffer_size = vertex_buffer.size() + (index_buffer.size() * sizeof(uint16_t)) + sizeof(vk::AabbPositionsKHR);

            auto staging_buffer = engine->renderer->gpu->memory_manager->GetBuffer(staging_buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            uint8_t* staging_buffer_data = static_cast<uint8_t*>(staging_buffer->map(0, staging_buffer_size, {}));

            auto command_buffer = std::move(command_buffers[0]);

            vk::CommandBufferBeginInfo begin_info = {};
            begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

            command_buffer->begin(begin_info);

            //particles may billboard, so the AABB must be able to contain any transformation matrix
            vk::AabbPositionsKHR aabbs_positions{ -aabb_dist, -aabb_dist, -aabb_dist, aabb_dist, aabb_dist, aabb_dist };

            memcpy(staging_buffer_data, vertex_buffer.data(), vertex_buffer.size());
            memcpy(staging_buffer_data + vertex_buffer.size(), index_buffer.data(), index_buffer.size() * sizeof(uint16_t));
            memcpy(staging_buffer_data + vertex_buffer.size() + (index_buffer.size() * sizeof(uint16_t)), &aabbs_positions, sizeof(vk::AabbPositionsKHR));

            vk::BufferCopy copy_region;
            copy_region.srcOffset = 0;
            copy_region.size = vertex_buffer.size();
            command_buffer->copyBuffer(staging_buffer->buffer, mesh->vertex_buffer->buffer, copy_region);
            copy_region.size = index_buffer.size() * sizeof(uint16_t);
            copy_region.srcOffset = vertex_buffer.size();
            command_buffer->copyBuffer(staging_buffer->buffer, mesh->index_buffer->buffer, copy_region);
            copy_region.size = sizeof(vk::AabbPositionsKHR);
            copy_region.srcOffset = vertex_buffer.size() + (index_buffer.size() * sizeof(uint16_t));
            command_buffer->copyBuffer(staging_buffer->buffer, mesh->aabbs_buffer->buffer, copy_region);

            if (engine->config->renderer.RaytraceEnabled())
            {
                raytrace_geometry.emplace_back(vk::GeometryTypeKHR::eAabbs, vk::AccelerationStructureGeometryAabbsDataKHR{
                    engine->renderer->gpu->device->getBufferAddressKHR(mesh->aabbs_buffer->buffer),
                    sizeof(vk::AabbPositionsKHR)
                    }, mesh->has_transparency ? vk::GeometryFlagsKHR{} : vk::GeometryFlagBitsKHR::eOpaque);

                raytrace_offset_info.emplace_back(1, 0);

                raytrace_create_info.emplace_back(vk::GeometryTypeKHR::eAabbs, 1,
                    vk::IndexType::eUint16, static_cast<uint32_t>(vertex_buffer.size() / vertex_stride), vk::Format::eR32G32B32Sfloat, false);
            }

            staging_buffer->unmap();

            if (engine->config->renderer.RaytraceEnabled())
            {
                vk::MemoryBarrier barrier;
                barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eAccelerationStructureReadKHR;
                command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, barrier, nullptr, nullptr);
                bottom_level_as = std::make_unique<BottomLevelAccelerationStructure>(static_cast<RendererRaytraceBase*>(engine->renderer.get()), *command_buffer, std::move(raytrace_geometry), std::move(raytrace_offset_info),
                    std::move(raytrace_create_info), false, false, BottomLevelAccelerationStructure::Performance::FastTrace);
            }
            command_buffer->end();

            engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
            engine->worker_pool->gpuResource(std::move(staging_buffer), std::move(command_buffer));
        }
        co_return;
    }
}

