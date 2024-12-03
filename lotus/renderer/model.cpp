module;

#include <coroutine>
#include <cstdint>
#include <cstring>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

module lotus;

import :renderer.model;

import :core.engine;
import :renderer.vulkan.renderer;
import vulkan_hpp;

namespace lotus
{
Model::Model(const std::string& _name) : name(_name) {}

WorkerTask<> Model::InitWork(Engine* engine, const std::vector<std::span<const std::byte>>& vertex_buffers,
                             const std::vector<std::span<const std::byte>>& index_buffers, uint32_t vertex_stride, std::vector<TransformEntry>&& transforms)
{
    // priority: -1
    if (!vertex_buffers.empty())
    {
        std::vector<vk::AccelerationStructureGeometryKHR> raytrace_geometry;
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> raytrace_offset_info;
        std::vector<uint32_t> max_primitive_count;
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *engine->renderer->compute_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

        vk::DeviceSize vertex_buffer_size = 0;
        vk::DeviceSize index_buffer_size = 0;
        vk::DeviceSize transform_buffer_size = (transforms.size() * sizeof(float) * 12);
        for (const auto& [vertex_buffer, index_buffer] : std::ranges::views::zip(vertex_buffers, index_buffers))
        {
            vertex_buffer_size += vertex_buffer.size();
            index_buffer_size += index_buffer.size();
        }
        auto staging_buffer_size = vertex_buffer_size + index_buffer_size + transform_buffer_size;

        auto staging_buffer = engine->renderer->gpu->memory_manager->GetBuffer(
            staging_buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        uint8_t* staging_buffer_data = static_cast<uint8_t*>(staging_buffer->map(0, staging_buffer_size, {}));

        auto vertex_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
        auto index_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;
        auto transform_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer;

        if (engine->config->renderer.RaytraceEnabled())
        {
            vertex_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                  vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
            index_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                 vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
            transform_usage_flags |= vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
        }
        vertex_buffer = engine->renderer->gpu->memory_manager->GetBuffer(vertex_buffer_size, vertex_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        index_buffer = engine->renderer->gpu->memory_manager->GetBuffer(index_buffer_size, index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        transform_buffer =
            engine->renderer->gpu->memory_manager->GetAlignedBuffer(transform_buffer_size, 16, index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::DeviceSize vertex_offset = 0;
        vk::DeviceSize index_offset = 0;
        vk::DeviceSize transform_offset = 0;

        bool transform_data = transforms.size() > 0;

        for (uint32_t i = 0; i < meshes.size(); ++i)
        {
            const auto& vertex_buffer = vertex_buffers[i];
            const auto& index_buffer = index_buffers[i];
            auto& mesh = meshes[i];

            memcpy(staging_buffer_data + vertex_offset, vertex_buffer.data(), vertex_buffer.size());
            memcpy(staging_buffer_data + vertex_buffer_size + index_offset, index_buffer.data(), index_buffer.size());

            mesh->vertex_offset = vertex_offset;
            mesh->vertex_size = vertex_buffer.size();
            mesh->index_offset = index_offset;
            mesh->index_size = index_buffer.size();

            if (!transform_data)
                transforms.push_back({.mesh_index = static_cast<uint32_t>(i)});

            vertex_offset += vertex_buffer.size();
            index_offset += index_buffer.size();
        }

        auto command_buffer = std::move(command_buffers[0]);
        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        command_buffer->begin(begin_info);

        for (const auto& transform : transforms)
        {
            const auto i = transform.mesh_index;
            auto& mesh = meshes[i];

            if (transform_data)
                memcpy(staging_buffer_data + vertex_buffer_size + index_buffer_size + transform_offset, &transform.transform, sizeof(float) * 12);

            mesh->transform_offset = transform_offset;

            if (engine->config->renderer.RaytraceEnabled() && !weighted)
            {
                raytrace_geometry.push_back(
                    {.geometryType = vk::GeometryTypeKHR::eTriangles,
                     .geometry = {.triangles =
                                      vk::AccelerationStructureGeometryTrianglesDataKHR{
                                          .vertexFormat = vk::Format::eR32G32B32Sfloat,
                                          .vertexData = engine->renderer->gpu->device->getBufferAddress({.buffer = vertex_buffer->buffer}),
                                          .vertexStride = vertex_stride,
                                          .maxVertex = mesh->getMaxIndex(),
                                          .indexType = vk::IndexType::eUint16,
                                          .indexData = engine->renderer->gpu->device->getBufferAddress({.buffer = index_buffer->buffer}),
                                          .transformData =
                                              transform_data ? engine->renderer->gpu->device->getBufferAddress({.buffer = transform_buffer->buffer}) : 0}},
                     .flags = mesh->has_transparency ? vk::GeometryFlagsKHR{} : vk::GeometryFlagBitsKHR::eOpaque});

                raytrace_offset_info.push_back({.primitiveCount = static_cast<uint32_t>(mesh->index_size / sizeof(uint16_t)) / 3,
                                                .primitiveOffset = static_cast<uint32_t>(mesh->index_offset),
                                                .firstVertex = static_cast<uint32_t>(mesh->vertex_offset / vertex_stride),
                                                .transformOffset = static_cast<uint32_t>(mesh->transform_offset)});
                max_primitive_count.emplace_back(static_cast<uint32_t>((mesh->index_size / sizeof(uint16_t)) / 3));
            }

            if (transform_data)
                transform_offset += sizeof(float) * 12;
        }

        auto copy = vk::BufferCopy{.srcOffset = 0, .size = vertex_buffer_size};
        command_buffer->copyBuffer(staging_buffer->buffer, vertex_buffer->buffer, copy);
        copy = vk::BufferCopy{.srcOffset = vertex_buffer_size, .size = index_buffer_size};
        command_buffer->copyBuffer(staging_buffer->buffer, index_buffer->buffer, copy);
        if (transform_data)
        {
            copy = vk::BufferCopy{.srcOffset = vertex_buffer_size + index_buffer_size, .size = transform_buffer_size};
            command_buffer->copyBuffer(staging_buffer->buffer, transform_buffer->buffer, copy);
        }

        staging_buffer->unmap();

        if (engine->config->renderer.RaytraceEnabled() && !weighted)
        {
            Renderer* renderer = engine->renderer.get();
            // TODO: test if just buffermemorybarrier is faster
            vk::MemoryBarrier2KHR barrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
                .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR | vk::AccessFlagBits2::eAccelerationStructureReadKHR |
                                 vk::AccessFlagBits2::eTransferRead,
            };

            command_buffer->pipelineBarrier2KHR({.memoryBarrierCount = 1, .pMemoryBarriers = &barrier});

            bottom_level_as = std::make_unique<BottomLevelAccelerationStructure>(
                renderer, *command_buffer, std::move(raytrace_geometry), std::move(raytrace_offset_info), std::move(max_primitive_count), false,
                lifetime == Lifetime::Long, BottomLevelAccelerationStructure::Performance::FastTrace);
        }
        command_buffer->end();

        // engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
        co_await engine->renderer->async_compute->compute(std::move(command_buffer));
        // engine->worker_pool->gpuResource(std::move(staging_buffer), std::move(command_buffer));
    }
    co_return;
}

WorkerTask<> Model::InitWorkAABB(Engine* engine, std::vector<uint8_t>&& vertices, std::vector<uint16_t>&& indices, uint32_t vertex_stride, float aabb_dist)
{
    if (!vertices.empty())
    {
        std::vector<vk::AccelerationStructureGeometryKHR> raytrace_geometry;
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> raytrace_offset_info;
        std::vector<uint32_t> max_primitives;
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *engine->renderer->compute_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

        // assumes only 1 mesh
        auto& mesh = meshes[0];

        vk::DeviceSize staging_buffer_size = vertices.size() + (indices.size() * sizeof(uint16_t)) + sizeof(vk::AabbPositionsKHR);

        auto staging_buffer = engine->renderer->gpu->memory_manager->GetBuffer(
            staging_buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        uint8_t* staging_buffer_data = static_cast<uint8_t*>(staging_buffer->map(0, staging_buffer_size, {}));

        vk::BufferUsageFlags vertex_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
        vk::BufferUsageFlags index_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;
        vk::BufferUsageFlags aabbs_usage_flags = vk::BufferUsageFlagBits::eTransferDst;

        if (engine->config->renderer.RaytraceEnabled())
        {
            vertex_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                  vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
            index_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                 vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
            aabbs_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                 vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
        }
        vertex_buffer = engine->renderer->gpu->memory_manager->GetBuffer(vertices.size(), vertex_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        index_buffer =
            engine->renderer->gpu->memory_manager->GetBuffer(indices.size() * sizeof(uint16_t), index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        aabbs_buffer =
            engine->renderer->gpu->memory_manager->GetBuffer(sizeof(vk::AabbPositionsKHR), aabbs_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);

        auto command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info);

        // particles may billboard, so the AABB must be able to contain any transformation matrix
        vk::AabbPositionsKHR aabbs_positions{-aabb_dist, -aabb_dist, -aabb_dist, aabb_dist, aabb_dist, aabb_dist};

        memcpy(staging_buffer_data, vertices.data(), vertices.size());
        memcpy(staging_buffer_data + vertices.size(), indices.data(), indices.size() * sizeof(uint16_t));
        memcpy(staging_buffer_data + vertices.size() + (indices.size() * sizeof(uint16_t)), &aabbs_positions, sizeof(vk::AabbPositionsKHR));

        auto copy = vk::BufferCopy{.srcOffset = 0, .size = vertices.size()};
        command_buffer->copyBuffer(staging_buffer->buffer, vertex_buffer->buffer, copy);
        copy = vk::BufferCopy{.srcOffset = vertices.size(), .size = indices.size() * sizeof(uint16_t)};
        command_buffer->copyBuffer(staging_buffer->buffer, index_buffer->buffer, copy);
        copy = vk::BufferCopy{.srcOffset = vertices.size() + (indices.size() * sizeof(uint16_t)), .size = sizeof(vk::AabbPositionsKHR)};
        command_buffer->copyBuffer(staging_buffer->buffer, aabbs_buffer->buffer, copy);

        if (engine->config->renderer.RaytraceEnabled())
        {
            raytrace_geometry.push_back(
                {.geometryType = vk::GeometryTypeKHR::eAabbs,
                 .geometry = {.aabbs = vk::AccelerationStructureGeometryAabbsDataKHR{.data = engine->renderer->gpu->device->getBufferAddress(
                                                                                         {.buffer = aabbs_buffer->buffer}),
                                                                                     .stride = sizeof(vk::AabbPositionsKHR)}},
                 .flags = mesh->has_transparency ? vk::GeometryFlagsKHR{} : vk::GeometryFlagBitsKHR::eOpaque});

            // 1 AABB
            raytrace_offset_info.push_back({.primitiveCount = 1});
            max_primitives.emplace_back(1);
        }

        staging_buffer->unmap();

        if (engine->config->renderer.RaytraceEnabled())
        {
            vk::MemoryBarrier2KHR barrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
                .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR | vk::AccessFlagBits2::eAccelerationStructureReadKHR |
                                 vk::AccessFlagBits2::eTransferRead,
            };

            command_buffer->pipelineBarrier2KHR({.memoryBarrierCount = 1, .pMemoryBarriers = &barrier});
            bottom_level_as = std::make_unique<BottomLevelAccelerationStructure>(engine->renderer.get(), *command_buffer, std::move(raytrace_geometry),
                                                                                 std::move(raytrace_offset_info), std::move(max_primitives), false, false,
                                                                                 BottomLevelAccelerationStructure::Performance::FastTrace);
        }
        command_buffer->end();

        co_await engine->renderer->async_compute->compute(std::move(command_buffer));
    }
    co_return;
}
} // namespace lotus
