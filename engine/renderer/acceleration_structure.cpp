#include "acceleration_structure.h"
#include "engine/core.h"
#include "engine/renderer/model.h"
#include "engine/renderer/vulkan/renderer_raytrace_base.h"
#include "engine/entity/deformable_entity.h"
#include "engine/entity/particle.h"
#include "engine/entity/component/animation_component.h"

namespace lotus
{
    void AccelerationStructure::PopulateAccelerationStructure(const std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR>& geometries)
    {
        vk::AccelerationStructureCreateInfoKHR info;
        info.type = type;
        info.flags = flags;
        info.maxGeometryCount = geometries.size();
        info.pGeometryInfos = geometries.data();
        info.compactedSize = 0;
        acceleration_structure = renderer->gpu->device->createAccelerationStructureKHRUnique(info, nullptr);
    }

    void AccelerationStructure::PopulateBuffers()
    {
        vk::AccelerationStructureMemoryRequirementsInfoKHR memory_requirements_info{vk::AccelerationStructureMemoryRequirementsTypeKHR::eBuildScratch, vk::AccelerationStructureBuildTypeKHR::eDevice, *acceleration_structure };
        memory_requirements_info.type = vk::AccelerationStructureMemoryRequirementsTypeKHR::eBuildScratch;

        auto memory_requirements_build = renderer->gpu->device->getAccelerationStructureMemoryRequirementsKHR(memory_requirements_info);

        memory_requirements_info.type = vk::AccelerationStructureMemoryRequirementsTypeKHR::eUpdateScratch;

        auto memory_requirements_update = renderer->gpu->device->getAccelerationStructureMemoryRequirementsKHR(memory_requirements_info);

        memory_requirements_info.type = vk::AccelerationStructureMemoryRequirementsTypeKHR::eObject;

        auto memory_requirements_object = renderer->gpu->device->getAccelerationStructureMemoryRequirementsKHR(memory_requirements_info);

        scratch_memory = renderer->gpu->memory_manager->GetBuffer(memory_requirements_build.memoryRequirements.size > memory_requirements_update.memoryRequirements.size ?
            memory_requirements_build.memoryRequirements.size : memory_requirements_update.memoryRequirements.size, vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal);

        object_memory = renderer->gpu->memory_manager->GetMemory(memory_requirements_object.memoryRequirements, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::BindAccelerationStructureMemoryInfoKHR bind_info;
        bind_info.accelerationStructure = *acceleration_structure;
        bind_info.memory = object_memory->get_memory();
        bind_info.memoryOffset = object_memory->get_memory_offset();
        renderer->gpu->device->bindAccelerationStructureMemoryKHR(bind_info);
        handle = renderer->gpu->device->getAccelerationStructureAddressKHR(*acceleration_structure);
    }

    void AccelerationStructure::UpdateAccelerationStructure(vk::CommandBuffer command_buffer,
        const std::vector<vk::AccelerationStructureGeometryKHR>& geometries,
        const std::vector<vk::AccelerationStructureBuildOffsetInfoKHR>& offsets)
    {
        BuildAccelerationStructure(command_buffer, geometries, offsets, true);
    }

    void AccelerationStructure::BuildAccelerationStructure(vk::CommandBuffer command_buffer, const std::vector<vk::AccelerationStructureGeometryKHR>& geometries,
        const std::vector<vk::AccelerationStructureBuildOffsetInfoKHR>& offsets, bool update)
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

        vk::DeviceOrHostAddressKHR scratch_data{renderer->gpu->device->getBufferAddressKHR(scratch_memory->buffer)};
        const auto geometry_p = geometries.data();
        vk::AccelerationStructureBuildGeometryInfoKHR build_info( type, flags, update, update ? *acceleration_structure : nullptr, *acceleration_structure, false, static_cast<uint32_t>(geometries.size()), &geometry_p, scratch_data );

        command_buffer.buildAccelerationStructureKHR(build_info, offsets.data());
    }

    void AccelerationStructure::Copy(vk::CommandBuffer command_buffer, AccelerationStructure& target)
    {
        vk::CopyAccelerationStructureInfoKHR info{*acceleration_structure, *target.acceleration_structure, vk::CopyAccelerationStructureModeKHR::eClone};
        command_buffer.copyAccelerationStructureKHR(info);
    }

    BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(RendererRaytraceBase* _renderer, class vk::CommandBuffer command_buffer, std::vector<vk::AccelerationStructureGeometryKHR>&& geometry,
        std::vector<vk::AccelerationStructureBuildOffsetInfoKHR>&& offsets, std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR>&& geometry_info,
        bool updateable, bool compact, Performance performance) : AccelerationStructure(_renderer, vk::AccelerationStructureTypeKHR::eBottomLevel)
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
        geometry_offsets = std::move(offsets);
        geometry_infos = std::move(geometry_info);
        PopulateAccelerationStructure(geometry_infos);
        PopulateBuffers();
        BuildAccelerationStructure(command_buffer, geometries, geometry_offsets, false);
    }

    void BottomLevelAccelerationStructure::Update(vk::CommandBuffer buffer)
    {
        UpdateAccelerationStructure(buffer, geometries, geometry_offsets);
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
                instance_memory = renderer->gpu->memory_manager->GetBuffer(instances.size() * sizeof(vk::AccelerationStructureInstanceKHR), vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
                vk::AccelerationStructureCreateGeometryTypeInfoKHR info{vk::GeometryTypeKHR::eInstances, static_cast<uint32_t>(instances.size())};
                info.allowsTransforms = true;
                std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR> infos{ info };
                PopulateAccelerationStructure(infos);
                PopulateBuffers();
                update = false;

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
            vk::AccelerationStructureGeometryKHR build_geometry{ vk::GeometryTypeKHR::eInstances,
                vk::AccelerationStructureGeometryInstancesDataKHR{ false, renderer->gpu->device->getBufferAddressKHR(instance_memory->buffer) } };
            std::vector<vk::AccelerationStructureGeometryKHR> instance_data_vec{ build_geometry };
            vk::AccelerationStructureBuildOffsetInfoKHR build_offset{static_cast<uint32_t>(instances.size()), 0, 0};
            std::vector<vk::AccelerationStructureBuildOffsetInfoKHR> instance_offset_vec{ build_offset };
            BuildAccelerationStructure(*command_buffer, instance_data_vec, instance_offset_vec, update);
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
