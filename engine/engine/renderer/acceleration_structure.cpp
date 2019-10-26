#include "acceleration_structure.h"
#include "engine/core.h"
#include "engine/renderer/model.h"
#include "engine/entity/renderable_entity.h"
#include "engine/entity/component/animation_component.h"

void lotus::AccelerationStructure::PopulateAccelerationStructure(uint32_t instanceCount,
    std::vector<vk::GeometryNV>* geometries, bool updateable)
{
    info.instanceCount = instanceCount;
    info.geometryCount = geometries ? geometries->size() : 0;
    info.pGeometries = geometries ? geometries->data() : nullptr;
    if (updateable)
    {
        info.flags |= vk::BuildAccelerationStructureFlagBitsNV::eAllowUpdate;
    }
    vk::AccelerationStructureCreateInfoNV create_info;
    create_info.info = info;
    create_info.compactedSize = 0;
    acceleration_structure = engine->renderer.device->createAccelerationStructureNVUnique(create_info, nullptr, engine->renderer.dispatch);
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
    engine->renderer.device->getAccelerationStructureHandleNV(*acceleration_structure, sizeof(handle), &handle, engine->renderer.dispatch);
}

void lotus::AccelerationStructure::UpdateAccelerationStructure(vk::CommandBuffer command_buffer,
    vk::Buffer instance_buffer, vk::DeviceSize instance_offset)
{
    BuildAccelerationStructure(command_buffer, instance_buffer, instance_offset, true);
}

void lotus::AccelerationStructure::BuildAccelerationStructure(vk::CommandBuffer command_buffer, vk::Buffer instance_buffer, vk::DeviceSize instance_offset, bool update)
{
    vk::BufferMemoryBarrier barrier;

    barrier.srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteNV | vk::AccessFlagBits::eAccelerationStructureReadNV;
    barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteNV | vk::AccessFlagBits::eAccelerationStructureReadNV;
    barrier.buffer = scratch_memory->buffer;

    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, vk::PipelineStageFlagBits::eAccelerationStructureBuildNV,
        {}, nullptr, barrier, nullptr, engine->renderer.dispatch);
    command_buffer.buildAccelerationStructureNV(info, instance_buffer, instance_offset, update, *acceleration_structure, update ? *acceleration_structure : VK_NULL_HANDLE, scratch_memory->buffer, 0, engine->renderer.dispatch);
}

void lotus::AccelerationStructure::Copy(vk::CommandBuffer command_buffer, AccelerationStructure& target)
{
    command_buffer.copyAccelerationStructureNV(*target.acceleration_structure, *acceleration_structure, vk::CopyAccelerationStructureModeNV::eClone, engine->renderer.dispatch);
}

lotus::BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(Engine* _engine, class vk::CommandBuffer command_buffer, const std::vector<vk::GeometryNV>& geometry, bool updateable, bool compact, Performance performance) : AccelerationStructure(_engine)
{
    info.type = vk::AccelerationStructureTypeNV::eBottomLevel;
    if (performance == Performance::FastBuild)
        info.flags |= vk::BuildAccelerationStructureFlagBitsNV::ePreferFastBuild;
    else if (performance == Performance::FastTrace)
        info.flags |= vk::BuildAccelerationStructureFlagBitsNV::ePreferFastTrace;
    if (compact)
        info.flags |= vk::BuildAccelerationStructureFlagBitsNV::eAllowCompaction;
    //TODO: compact
    geometries = geometry;
    PopulateAccelerationStructure(0, &geometries, updateable);
    PopulateBuffers();
    BuildAccelerationStructure(command_buffer, nullptr, 0, false);
}

void lotus::BottomLevelAccelerationStructure::Update(vk::CommandBuffer buffer)
{
    UpdateAccelerationStructure(buffer, nullptr, 0);
}

lotus::TopLevelAccelerationStructure::TopLevelAccelerationStructure(Engine* _engine, bool _updateable) : AccelerationStructure(_engine), updateable(_updateable)
{
    info.type = vk::AccelerationStructureTypeNV::eTopLevel;
}

uint32_t lotus::TopLevelAccelerationStructure::AddInstance(VkGeometryInstance instance)
{
    uint32_t instanceid = static_cast<uint32_t>(instances.size());
    instances.push_back(instance);
    dirty = true;
    return instanceid;
}

void lotus::TopLevelAccelerationStructure::Build(vk::CommandBuffer command_buffer)
{
    if (dirty)
    {
        bool update = true;
        if (!instance_memory)
        {
            instance_memory = engine->renderer.memory_manager->GetBuffer(instances.size() * sizeof(VkGeometryInstance), vk::BufferUsageFlagBits::eRayTracingNV, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            PopulateAccelerationStructure(static_cast<uint32_t>(instances.size()), nullptr, updateable);
            PopulateBuffers();
            update = false;

            vk::WriteDescriptorSet write_info_as;
            write_info_as.descriptorCount = 1;
            write_info_as.descriptorType = vk::DescriptorType::eAccelerationStructureNV;
            write_info_as.dstBinding = 0;
            write_info_as.dstArrayElement = 0;

            vk::WriteDescriptorSetAccelerationStructureNV write_as;
            write_as.accelerationStructureCount = 1;
            write_as.pAccelerationStructures = &*acceleration_structure;
            write_info_as.pNext = &write_as;

            vk::WriteDescriptorSet write_info_vertex;
            write_info_vertex.descriptorType = vk::DescriptorType::eStorageBuffer;
            write_info_vertex.dstArrayElement = 0;
            write_info_vertex.dstBinding = 1;
            write_info_vertex.descriptorCount = static_cast<uint32_t>(descriptor_vertex_info.size());
            write_info_vertex.pBufferInfo = descriptor_vertex_info.data();

            vk::WriteDescriptorSet write_info_index;
            write_info_index.descriptorType = vk::DescriptorType::eStorageBuffer;
            write_info_index.dstArrayElement = 0;
            write_info_index.dstBinding = 2;
            write_info_index.descriptorCount = static_cast<uint32_t>(descriptor_index_info.size());
            write_info_index.pBufferInfo = descriptor_index_info.data();

            vk::WriteDescriptorSet write_info_texture;
            write_info_texture.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            write_info_texture.dstArrayElement = 0;
            write_info_texture.dstBinding = 3;
            write_info_texture.descriptorCount = static_cast<uint32_t>(descriptor_texture_info.size());
            write_info_texture.pImageInfo = descriptor_texture_info.data();

            std::vector<vk::WriteDescriptorSet> writes;
            uint32_t i = engine->renderer.getCurrentImage();
            write_info_vertex.dstSet = *engine->renderer.rtx_descriptor_sets_const[i];
            write_info_index.dstSet = *engine->renderer.rtx_descriptor_sets_const[i];
            write_info_texture.dstSet = *engine->renderer.rtx_descriptor_sets_const[i];
            write_info_as.dstSet = *engine->renderer.rtx_descriptor_sets_const[i];
            writes.push_back(write_info_vertex);
            writes.push_back(write_info_index);
            writes.push_back(write_info_texture);
            writes.push_back(write_info_as);
            engine->renderer.device->updateDescriptorSets(writes, nullptr, engine->renderer.dispatch);
        }
        auto data = engine->renderer.device->mapMemory(instance_memory->memory, instance_memory->memory_offset, instances.size() * sizeof(VkGeometryInstance), {}, engine->renderer.dispatch);
        memcpy(data, instances.data(), instances.size() * sizeof(VkGeometryInstance));
        engine->renderer.device->unmapMemory(instance_memory->memory, engine->renderer.dispatch);

        vk::MemoryBarrier barrier;

        barrier.srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteNV | vk::AccessFlagBits::eAccelerationStructureReadNV;
        barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteNV | vk::AccessFlagBits::eAccelerationStructureReadNV;

        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, vk::PipelineStageFlagBits::eAccelerationStructureBuildNV,
            {}, barrier, nullptr, nullptr, engine->renderer.dispatch);
        BuildAccelerationStructure(command_buffer, instance_memory->buffer, 0, update);
        dirty = false;
    }
}

void lotus::TopLevelAccelerationStructure::UpdateInstance(uint32_t instance_id, glm::mat3x4 transform)
{
    if (instance_memory)
    {
        instances[instance_id].transform = transform;
        dirty = true;
    }
}

void lotus::TopLevelAccelerationStructure::AddBLASResource(Model* model)
{
    uint16_t index = static_cast<uint16_t>(descriptor_vertex_info.size());
    for (const auto& mesh : model->meshes)
    {
        descriptor_vertex_info.emplace_back(mesh->vertex_buffer->buffer, 0, VK_WHOLE_SIZE);
        descriptor_index_info.emplace_back(mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE);
        descriptor_texture_info.emplace_back(*mesh->texture->sampler, *mesh->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
    model->bottom_level_as->resource_index = index;
}

void lotus::TopLevelAccelerationStructure::AddBLASResource(RenderableEntity* entity)
{
    if (entity->animation_component)
    {
        uint32_t image = engine->renderer.getCurrentImage();
        for (size_t i = 0; i < entity->models.size(); ++i)
        {
            uint16_t index = static_cast<uint16_t>(descriptor_vertex_info.size());
            for (size_t j = 0; j < entity->models[i]->meshes.size(); ++j)
            {
                const auto& mesh = entity->models[i]->meshes[j];
                descriptor_vertex_info.emplace_back(entity->animation_component->acceleration_structures[i].vertex_buffers[j][image]->buffer, 0, VK_WHOLE_SIZE);
                descriptor_index_info.emplace_back(mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE);
                descriptor_texture_info.emplace_back(*mesh->texture->sampler, *mesh->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
            }
            entity->animation_component->acceleration_structures[i].bottom_level_as[image]->resource_index = index;
        }
    }
}
