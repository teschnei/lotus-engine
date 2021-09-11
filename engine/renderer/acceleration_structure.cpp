#include "acceleration_structure.h"
#include "engine/core.h"
#include "engine/renderer/model.h"
#include "engine/renderer/vulkan/renderer_raytrace_base.h"
#include "engine/entity/deformable_entity.h"
#include "engine/entity/particle.h"
#include "engine/entity/component/animation_component.h"

namespace lotus
{
    void AccelerationStructure::CreateAccelerationStructure(std::span<vk::AccelerationStructureGeometryKHR> geometries, std::span<uint32_t> max_primitive_counts)
    {
        vk::AccelerationStructureBuildGeometryInfoKHR build_info;
        build_info.type = type;
        build_info.flags = flags;
        build_info.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
        build_info.geometryCount = geometries.size();
        build_info.pGeometries = geometries.data();

        auto size_info = renderer->gpu->device->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, build_info, max_primitive_counts);

        scratch_memory = renderer->gpu->memory_manager->GetBuffer(size_info.buildScratchSize > size_info.updateScratchSize ?
            size_info.buildScratchSize : size_info.updateScratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal);

        object_memory = renderer->gpu->memory_manager->GetBuffer(size_info.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::AccelerationStructureCreateInfoKHR info;
        info.type = type;
        info.buffer = object_memory->buffer;
        info.size = size_info.accelerationStructureSize;
        acceleration_structure = renderer->gpu->device->createAccelerationStructureKHRUnique(info, nullptr);
        handle = renderer->gpu->device->getAccelerationStructureAddressKHR(*acceleration_structure);
    }

    void AccelerationStructure::UpdateAccelerationStructure(vk::CommandBuffer command_buffer,
        std::span<vk::AccelerationStructureGeometryKHR> geometries,
        std::span<vk::AccelerationStructureBuildRangeInfoKHR> ranges)
    {
        BuildAccelerationStructure(command_buffer, geometries, ranges, vk::BuildAccelerationStructureModeKHR::eUpdate);
    }

    void AccelerationStructure::BuildAccelerationStructure(vk::CommandBuffer command_buffer, std::span<vk::AccelerationStructureGeometryKHR> geometries,
        std::span<vk::AccelerationStructureBuildRangeInfoKHR> ranges, vk::BuildAccelerationStructureModeKHR mode)
    {
        vk::BufferMemoryBarrier barrier;

        barrier.srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eAccelerationStructureReadKHR;
        barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eAccelerationStructureReadKHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = scratch_memory->buffer;
        barrier.size = VK_WHOLE_SIZE;

        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
            {}, nullptr, barrier, nullptr);

        vk::DeviceOrHostAddressKHR scratch_data{renderer->gpu->device->getBufferAddress(scratch_memory->buffer)};
        vk::AccelerationStructureBuildGeometryInfoKHR build_info( type, flags, mode, *acceleration_structure, *acceleration_structure, static_cast<uint32_t>(geometries.size()), geometries.data(), nullptr, scratch_data );

        command_buffer.buildAccelerationStructuresKHR(build_info, ranges.data());
    }

    void AccelerationStructure::Copy(vk::CommandBuffer command_buffer, AccelerationStructure& target)
    {
        vk::CopyAccelerationStructureInfoKHR info{*acceleration_structure, *target.acceleration_structure, vk::CopyAccelerationStructureModeKHR::eClone};
        command_buffer.copyAccelerationStructureKHR(info);
    }

    BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(RendererRaytraceBase* _renderer, class vk::CommandBuffer command_buffer,
        std::vector<vk::AccelerationStructureGeometryKHR>&& geometry, std::vector<vk::AccelerationStructureBuildRangeInfoKHR>&& ranges,
        std::vector<uint32_t>&& primitive_counts, bool updateable, bool compact, Performance performance) :
        AccelerationStructure(_renderer, vk::AccelerationStructureTypeKHR::eBottomLevel)
    {
        if (performance == Performance::FastBuild)
            flags |= vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild;
        else if (performance == Performance::FastTrace)
            flags |= vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
        if (compact)
            flags |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction;
        if (updateable)
            flags |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
        //TODO: compact
        geometries = std::move(geometry);
        geometry_ranges = std::move(ranges);
        max_primitive_counts = std::move(primitive_counts);
        CreateAccelerationStructure(geometries, max_primitive_counts);
        BuildAccelerationStructure(command_buffer, geometries, geometry_ranges, vk::BuildAccelerationStructureModeKHR::eBuild);
    }

    void BottomLevelAccelerationStructure::Update(vk::CommandBuffer buffer)
    {
        UpdateAccelerationStructure(buffer, geometries, geometry_ranges);
    }

    TopLevelAccelerationStructure::TopLevelAccelerationStructure(RendererRaytraceBase* _renderer, bool _updateable) : AccelerationStructure(_renderer, vk::AccelerationStructureTypeKHR::eTopLevel), updateable(_updateable)
    {
        instances.reserve(GlobalResources::max_resource_index);
        if (updateable)
            flags |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
    }

    uint32_t TopLevelAccelerationStructure::AddInstance(vk::AccelerationStructureInstanceKHR instance)
    {
        uint32_t instanceid = static_cast<uint32_t>(instances.size());
        instances.push_back(instance);
        dirty = true;
        return instanceid;
    }

    WorkerTask<> TopLevelAccelerationStructure::Build(Engine* engine)
    {
        //priority: 2
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *engine->renderer->compute_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
        auto command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info);

        if (dirty)
        {
            bool update = true;
            uint32_t i = renderer->getCurrentImage();

            if (!instance_memory)
            {
                instance_memory = renderer->gpu->memory_manager->GetBuffer(instances.size() * sizeof(vk::AccelerationStructureInstanceKHR), vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
                update = false;
            }

            vk::AccelerationStructureGeometryKHR build_geometry{ vk::GeometryTypeKHR::eInstances,
                vk::AccelerationStructureGeometryInstancesDataKHR{ false, renderer->gpu->device->getBufferAddress(instance_memory->buffer) } };
            std::vector<vk::AccelerationStructureGeometryKHR> instance_data_vec{ build_geometry };
            vk::AccelerationStructureBuildRangeInfoKHR build_range{static_cast<uint32_t>(instances.size()), 0, 0};
            std::vector<vk::AccelerationStructureBuildRangeInfoKHR> instance_range_vec{ build_range };

            if (!update)
            {
                //info.allowsTransforms = true;
                std::vector<uint32_t> max_primitives { {static_cast<uint32_t>(instances.size())} };
                CreateAccelerationStructure(instance_data_vec, max_primitives);

                vk::WriteDescriptorSet write_info_as;
                write_info_as.descriptorCount = 1;
                write_info_as.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
                write_info_as.dstBinding = 0;
                write_info_as.dstArrayElement = 0;

                vk::WriteDescriptorSetAccelerationStructureKHR write_as;
                write_as.accelerationStructureCount = 1;
                write_as.pAccelerationStructures = &*acceleration_structure;
                write_info_as.pNext = &write_as;

                write_info_as.dstSet = *renderer->rtx_descriptor_sets_const[i];
                renderer->gpu->device->updateDescriptorSets({write_info_as}, nullptr);
            }
            auto data = instance_memory->map(0, instances.size() * sizeof(vk::AccelerationStructureInstanceKHR), {});
            memcpy(data, instances.data(), instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
            instance_memory->unmap();

            vk::MemoryBarrier barrier;

            barrier.srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eAccelerationStructureReadKHR;
            barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eAccelerationStructureReadKHR;

            command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
                {}, barrier, nullptr, nullptr);

            BuildAccelerationStructure(*command_buffer, instance_data_vec, instance_range_vec, update ? vk::BuildAccelerationStructureModeKHR::eUpdate : vk::BuildAccelerationStructureModeKHR::eBuild);
            dirty = false;
        }
        command_buffer->end();

        engine->worker_pool->command_buffers.compute.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(command_buffer));
        co_return;
    }

    void TopLevelAccelerationStructure::UpdateInstance(uint32_t instance_id, glm::mat3x4 transform)
    {
        if (instance_memory)
        {
            memcpy(&instances[instance_id].transform, &transform, sizeof(vk::TransformMatrixKHR));
            dirty = true;
        }
    }
}
