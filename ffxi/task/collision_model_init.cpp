#include "collision_model_init.h"
#include "engine/core.h"
#include "engine/worker_thread.h"
#include "engine/renderer/acceleration_structure.h"
#include "entity/landscape_entity.h"

CollisionModelInitTask::CollisionModelInitTask(std::shared_ptr<lotus::Model> _model, std::vector<FFXI::CollisionMeshData> _mesh_data, std::vector<FFXI::CollisionEntry> _entries, uint32_t _vertex_stride) :
    lotus::WorkItem(), model(_model), mesh_data(_mesh_data), entries(_entries), vertex_stride(_vertex_stride)
{
}

void CollisionModelInitTask::Process(lotus::WorkerThread* thread)
{
    std::vector<vk::DeviceSize> vertex_offsets;
    std::vector<vk::DeviceSize> index_offsets;

    CollisionMesh* mesh = static_cast<CollisionMesh*>(model->meshes[0].get());

    staging_buffer = thread->engine->renderer.memory_manager->GetBuffer(mesh->vertex_buffer->getSize() + mesh->index_buffer->getSize() + mesh->transform_buffer->getSize(),
        vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    uint8_t* staging_map = static_cast<uint8_t*>(staging_buffer->map(0, VK_WHOLE_SIZE, {}));

    vk::DeviceSize staging_vertex_offset = 0;
    vk::DeviceSize staging_index_offset = 0;

    for (const auto& data : mesh_data)
    {
        memcpy(staging_map + staging_vertex_offset, data.vertices.data(), data.vertices.size());
        vertex_offsets.push_back(staging_vertex_offset);
        staging_vertex_offset += data.vertices.size();
        memcpy(staging_map + mesh->vertex_buffer->getSize() + staging_index_offset, data.indices.data(), data.indices.size() * sizeof(uint16_t));
        index_offsets.push_back(staging_index_offset);
        staging_index_offset += data.indices.size() * sizeof(uint16_t);
    }

    std::vector<vk::AccelerationStructureGeometryKHR> raytrace_geometry;
    std::vector<vk::AccelerationStructureBuildOffsetInfoKHR> raytrace_offset_info;
    std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR> raytrace_create_info;

    if (thread->engine->renderer.RaytraceEnabled())
    {
        vk::DeviceSize transform_offset = 0;

        for (const auto& entry : entries)
        {
            glm::mat3x4 transform = entry.transform;
            memcpy(staging_map + mesh->vertex_buffer->getSize() + mesh->index_buffer->getSize() + transform_offset, &transform, sizeof(float) * 12);

            FFXI::CollisionMeshData& data = mesh_data[entry.mesh_entry];

            raytrace_geometry.emplace_back(vk::GeometryTypeKHR::eTriangles, vk::AccelerationStructureGeometryTrianglesDataKHR{
                vk::Format::eR32G32B32Sfloat,
                thread->engine->renderer.device->getBufferAddressKHR(mesh->vertex_buffer->buffer),
                vertex_stride,
                vk::IndexType::eUint16,
                thread->engine->renderer.device->getBufferAddressKHR(mesh->index_buffer->buffer),
                thread->engine->renderer.device->getBufferAddressKHR(mesh->transform_buffer->buffer)
                }, vk::GeometryFlagBitsKHR::eOpaque);

            raytrace_offset_info.emplace_back(static_cast<uint32_t>(data.indices.size() / 3), index_offsets[entry.mesh_entry], vertex_offsets[entry.mesh_entry] / vertex_stride, transform_offset);

            raytrace_create_info.emplace_back(vk::GeometryTypeKHR::eTriangles, static_cast<uint32_t>(data.indices.size() / 3),
                vk::IndexType::eUint16, static_cast<uint32_t>(data.vertices.size() / vertex_stride), vk::Format::eR32G32B32Sfloat, true);

            transform_offset += sizeof(float) * 12;
        }
    }

    vk::CommandBufferAllocateInfo alloc_info = {};
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandPool = *thread->compute_pool;
    alloc_info.commandBufferCount = 1;

    auto command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);

    command_buffer = std::move(command_buffers[0]);

    vk::CommandBufferBeginInfo begin_info = {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    command_buffer->begin(begin_info, thread->engine->renderer.dispatch);

    vk::BufferCopy copy_region;
    copy_region.srcOffset = 0;
    copy_region.size = mesh->vertex_buffer->getSize();
    command_buffer->copyBuffer(staging_buffer->buffer, mesh->vertex_buffer->buffer, copy_region);

    copy_region.srcOffset = mesh->vertex_buffer->getSize();
    copy_region.size = mesh->index_buffer->getSize();
    command_buffer->copyBuffer(staging_buffer->buffer, mesh->index_buffer->buffer, copy_region);

    copy_region.srcOffset = mesh->vertex_buffer->getSize() + mesh->index_buffer->getSize();
    copy_region.size = mesh->transform_buffer->getSize();
    command_buffer->copyBuffer(staging_buffer->buffer, mesh->transform_buffer->buffer, copy_region);

    staging_buffer->unmap();

    if (thread->engine->renderer.RaytraceEnabled())
    {
        vk::MemoryBarrier barrier;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eAccelerationStructureReadKHR;
        command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, barrier, nullptr, nullptr, thread->engine->renderer.dispatch);
        model->bottom_level_as = std::make_unique<lotus::BottomLevelAccelerationStructure>(thread->engine, *command_buffer, std::move(raytrace_geometry), std::move(raytrace_offset_info),
            std::move(raytrace_create_info), false, true, lotus::BottomLevelAccelerationStructure::Performance::FastTrace);
    }

    command_buffer->end(thread->engine->renderer.dispatch);

    compute.primary = *command_buffer;
}
