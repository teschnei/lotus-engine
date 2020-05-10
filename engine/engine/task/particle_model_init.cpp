#include "particle_model_init.h"
#include <utility>
#include <numeric>
#include "../worker_thread.h"
#include "../core.h"

namespace lotus
{
    ParticleModelInitTask::ParticleModelInitTask(int _image_index, std::shared_ptr<Model> _model, std::vector<uint8_t>&& _vertex_buffer, uint32_t _vertex_stride, float _aabb_dist) :
        WorkItem(), image_index(_image_index), model(std::move(_model)), vertex_buffer(std::move(_vertex_buffer)), vertex_stride(_vertex_stride), aabb_dist(_aabb_dist)
    {
        priority = -1;
    }

    void ParticleModelInitTask::Process(WorkerThread* thread)
    {
        if (!vertex_buffer.empty())
        {
            std::vector<vk::AccelerationStructureGeometryKHR> raytrace_geometry;
            std::vector<vk::AccelerationStructureBuildOffsetInfoKHR> raytrace_offset_info;
            std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR> raytrace_create_info;
            vk::CommandBufferAllocateInfo alloc_info = {};
            alloc_info.level = vk::CommandBufferLevel::ePrimary;
            alloc_info.commandPool = *thread->graphics_pool;
            alloc_info.commandBufferCount = 1;

            auto command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info);

            //assumes only 1 mesh
            auto& mesh = model->meshes[0];

            auto index_buffer = std::vector<uint16_t>(mesh->getIndexCount());
            std::iota(index_buffer.begin(), index_buffer.end(), 0);

            vk::DeviceSize staging_buffer_size = vertex_buffer.size() + (index_buffer.size() * sizeof(uint16_t)) + sizeof(vk::AabbPositionsKHR);

            staging_buffer = thread->engine->renderer.memory_manager->GetBuffer(staging_buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            uint8_t* staging_buffer_data = static_cast<uint8_t*>(staging_buffer->map(0, staging_buffer_size, {}));

            command_buffer = std::move(command_buffers[0]);

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

            if (thread->engine->renderer.RaytraceEnabled())
            {
                raytrace_geometry.emplace_back(vk::GeometryTypeKHR::eAabbs, vk::AccelerationStructureGeometryAabbsDataKHR{
                    thread->engine->renderer.device->getBufferAddressKHR(mesh->aabbs_buffer->buffer),
                    sizeof(vk::AabbPositionsKHR)
                    }, mesh->has_transparency ? vk::GeometryFlagsKHR{} : vk::GeometryFlagBitsKHR::eOpaque);

                raytrace_offset_info.emplace_back(1, 0);

                raytrace_create_info.emplace_back(vk::GeometryTypeKHR::eAabbs, 1,
                    vk::IndexType::eUint16, static_cast<uint32_t>(vertex_buffer.size() / vertex_stride), vk::Format::eR32G32B32Sfloat, false);
            }

            staging_buffer->unmap();

            if (thread->engine->renderer.RaytraceEnabled())
            {
                vk::MemoryBarrier barrier;
                barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eAccelerationStructureReadKHR;
                command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, barrier, nullptr, nullptr);
                model->bottom_level_as = std::make_unique<BottomLevelAccelerationStructure>(thread->engine, *command_buffer, std::move(raytrace_geometry), std::move(raytrace_offset_info),
                    std::move(raytrace_create_info), false, false, BottomLevelAccelerationStructure::Performance::FastTrace);
            }
            command_buffer->end();

            graphics.primary = *command_buffer;
        }
    }
}