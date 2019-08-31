#include "acceleration_structure.h"
#include "core.h"

void lotus::AccelerationStructure::PopulateAccelerationStructure(vk::DeviceSize instanceCount,
    vk::DeviceSize geometryCount, const vk::GeometryNV* pGeometry, bool updateable)
{
    info.instanceCount = instanceCount;
    info.geometryCount = geometryCount;
    info.pGeometries = pGeometry;
    info.flags = {};
    if (updateable)
    {
        info.flags |= vk::BuildAccelerationStructureFlagBitsNV::eAllowUpdate;
    }
    vk::AccelerationStructureCreateInfoNV create_info;
    create_info.info = info;
    create_info.compactedSize = 0;
    acceleration_structure = engine->renderer.device->createAccelerationStructureNVUnique(create_info, nullptr, engine->renderer.dispatch);
    engine->renderer.device->getAccelerationStructureHandleNV(*acceleration_structure, sizeof(handle), &handle, engine->renderer.dispatch);
}

void lotus::AccelerationStructure::PopulateBuffers()
{
    vk::AccelerationStructureMemoryRequirementsInfoNV memory_requirements_info;
    memory_requirements_info.accelerationStructure = *acceleration_structure;
    memory_requirements_info.type = vk::AccelerationStructureMemoryRequirementsTypeNV::eBuildScratch;

    auto memory_requirements_build = engine->renderer.device->getAccelerationStructureMemoryRequirementsNV(memory_requirements_info, engine->renderer.dispatch);

    memory_requirements_info.type = vk::AccelerationStructureMemoryRequirementsTypeNV::eUpdateScratch;

    auto memory_requirements_update = engine->renderer.device->getAccelerationStructureMemoryRequirementsNV(memory_requirements_info, engine->renderer.dispatch);

    memory_requirements_info.type = vk::AccelerationStructureMemoryRequirementsTypeNV::eObject;

    auto memory_requirements_object = engine->renderer.device->getAccelerationStructureMemoryRequirementsNV(memory_requirements_info, engine->renderer.dispatch);

    scratch_memory = engine->renderer.memory_manager->GetBuffer(memory_requirements_build.memoryRequirements.size > memory_requirements_update.memoryRequirements.size ?
        memory_requirements_build.memoryRequirements.size : memory_requirements_update.memoryRequirements.size, vk::BufferUsageFlagBits::eRayTracingNV, vk::MemoryPropertyFlagBits::eDeviceLocal);
    object_memory = engine->renderer.memory_manager->GetMemory(memory_requirements_object.memoryRequirements, vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::BindAccelerationStructureMemoryInfoNV bind_info;
    bind_info.accelerationStructure = *acceleration_structure;
    bind_info.memory = object_memory->memory;
    bind_info.memoryOffset = object_memory->memory_offset;
    engine->renderer.device->bindAccelerationStructureMemoryNV(bind_info, engine->renderer.dispatch);
}

void lotus::AccelerationStructure::UpdateAccelerationStructure(vk::CommandBuffer command_buffer,
    vk::Buffer instance_buffer, vk::DeviceSize instance_offset)
{
    BuildAccelerationStructure(command_buffer, instance_buffer, instance_offset, true);
}

void lotus::AccelerationStructure::BuildAccelerationStructure(vk::CommandBuffer command_buffer, vk::Buffer instance_buffer, vk::DeviceSize instance_offset, bool update)
{
    command_buffer.buildAccelerationStructureNV(info, instance_buffer, instance_offset, update, *acceleration_structure, update ? *acceleration_structure : VK_NULL_HANDLE, scratch_memory->buffer, 0, engine->renderer.dispatch);
}

lotus::BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(Engine* _engine, vk::CommandBuffer command_buffer, const std::vector<vk::GeometryNV>& geometry, bool updateable) : AccelerationStructure(_engine)
{
    info.type = vk::AccelerationStructureTypeNV::eBottomLevel;
    PopulateAccelerationStructure(0, geometry.size(), geometry.data(), updateable);
    PopulateBuffers();
    BuildAccelerationStructure(command_buffer, nullptr, 0, false);
}

lotus::TopLevelAccelerationStructure::TopLevelAccelerationStructure(Engine* _engine, bool _updateable) : AccelerationStructure(_engine), updateable(_updateable)
{
    info.type = vk::AccelerationStructureTypeNV::eTopLevel;
}

void lotus::TopLevelAccelerationStructure::AddInstance(VkGeometryInstance instance)
{
    instances.push_back(instance);
    dirty = true;
}

void lotus::TopLevelAccelerationStructure::Build(vk::CommandBuffer command_buffer)
{
    if (dirty)
    {
        bool update = true;
        if (!instance_memory)
        {
            instance_memory = engine->renderer.memory_manager->GetBuffer(instances.size() * sizeof(VkGeometryInstance), vk::BufferUsageFlagBits::eRayTracingNV, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            PopulateAccelerationStructure(instances.size(), 0, nullptr, updateable);
            PopulateBuffers();
            update = false;
        }
        auto data = engine->renderer.device->mapMemory(instance_memory->memory, instance_memory->memory_offset, instances.size() & sizeof(VkGeometryInstance), {}, engine->renderer.dispatch);
        memcpy(data, instances.data(), instances.size() * sizeof(VkGeometryInstance));
        engine->renderer.device->unmapMemory(instance_memory->memory, engine->renderer.dispatch);
        vk::MemoryBarrier barrier;

        barrier.srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteNV | vk::AccessFlagBits::eAccelerationStructureReadNV;
        barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteNV | vk::AccessFlagBits::eAccelerationStructureReadNV;

        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, vk::PipelineStageFlagBits::eAccelerationStructureBuildNV,
            {}, barrier, nullptr, nullptr, engine->renderer.dispatch);
        BuildAccelerationStructure(command_buffer, instance_memory->buffer, 0, update);
    }
}

void lotus::TopLevelAccelerationStructure::UpdateInstance(uint32_t instance_id, float transform[12])
{
    memcpy(&instances[instance_id].transform, transform, sizeof(float) * 12);
    dirty = true;
}
