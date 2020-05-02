#include "acceleration_structure.h"
#include "engine/core.h"
#include "engine/renderer/model.h"
#include "engine/entity/deformable_entity.h"
#include "engine/entity/particle.h"
#include "engine/entity/component/animation_component.h"

void lotus::AccelerationStructure::PopulateAccelerationStructure(const std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR>& geometries)
{
    vk::AccelerationStructureCreateInfoKHR info;
    info.type = type;
    info.flags = flags;
    info.maxGeometryCount = geometries.size();
    info.pGeometryInfos = geometries.data();
    info.compactedSize = 0;
    acceleration_structure = engine->renderer.device->createAccelerationStructureKHRUnique(info, nullptr);
}

void lotus::AccelerationStructure::PopulateBuffers()
{
    vk::AccelerationStructureMemoryRequirementsInfoKHR memory_requirements_info{vk::AccelerationStructureMemoryRequirementsTypeKHR::eBuildScratch, vk::AccelerationStructureBuildTypeKHR::eDevice, *acceleration_structure };
    memory_requirements_info.type = vk::AccelerationStructureMemoryRequirementsTypeKHR::eBuildScratch;

    auto memory_requirements_build = engine->renderer.device->getAccelerationStructureMemoryRequirementsKHR(memory_requirements_info);

    memory_requirements_info.type = vk::AccelerationStructureMemoryRequirementsTypeKHR::eUpdateScratch;

    auto memory_requirements_update = engine->renderer.device->getAccelerationStructureMemoryRequirementsKHR(memory_requirements_info);

    memory_requirements_info.type = vk::AccelerationStructureMemoryRequirementsTypeKHR::eObject;

    auto memory_requirements_object = engine->renderer.device->getAccelerationStructureMemoryRequirementsKHR(memory_requirements_info);

    scratch_memory = engine->renderer.memory_manager->GetBuffer(memory_requirements_build.memoryRequirements.size > memory_requirements_update.memoryRequirements.size ?
        memory_requirements_build.memoryRequirements.size : memory_requirements_update.memoryRequirements.size, vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal);

    object_memory = engine->renderer.memory_manager->GetMemory(memory_requirements_object.memoryRequirements, vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::BindAccelerationStructureMemoryInfoKHR bind_info;
    bind_info.accelerationStructure = *acceleration_structure;
    bind_info.memory = object_memory->get_memory();
    bind_info.memoryOffset = object_memory->get_memory_offset();
    engine->renderer.device->bindAccelerationStructureMemoryKHR(bind_info);
    handle = engine->renderer.device->getAccelerationStructureAddressKHR(*acceleration_structure);
}

void lotus::AccelerationStructure::UpdateAccelerationStructure(vk::CommandBuffer command_buffer,
    const std::vector<vk::AccelerationStructureGeometryKHR>& geometries,
    const std::vector<vk::AccelerationStructureBuildOffsetInfoKHR>& offsets)
{
    BuildAccelerationStructure(command_buffer, geometries, offsets, true);
}

void lotus::AccelerationStructure::BuildAccelerationStructure(vk::CommandBuffer command_buffer, const std::vector<vk::AccelerationStructureGeometryKHR>& geometries,
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

    vk::DeviceOrHostAddressKHR scratch_data{engine->renderer.device->getBufferAddressKHR(scratch_memory->buffer)};
    const auto geometry_p = geometries.data();
    vk::AccelerationStructureBuildGeometryInfoKHR build_info{ type, flags, update, update ? *acceleration_structure : VK_NULL_HANDLE, *acceleration_structure, false, static_cast<uint32_t>(geometries.size()), &geometry_p, scratch_data };

    command_buffer.buildAccelerationStructureKHR(build_info, offsets.data());
}

void lotus::AccelerationStructure::Copy(vk::CommandBuffer command_buffer, AccelerationStructure& target)
{
    vk::CopyAccelerationStructureInfoKHR info{*acceleration_structure, *target.acceleration_structure, vk::CopyAccelerationStructureModeKHR::eClone};
    command_buffer.copyAccelerationStructureKHR(info);
}

lotus::BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(Engine* _engine, class vk::CommandBuffer command_buffer, std::vector<vk::AccelerationStructureGeometryKHR>&& geometry,
    std::vector<vk::AccelerationStructureBuildOffsetInfoKHR>&& offsets, std::vector<vk::AccelerationStructureCreateGeometryTypeInfoKHR>&& geometry_info,
    bool updateable, bool compact, Performance performance) : AccelerationStructure(_engine, vk::AccelerationStructureTypeKHR::eBottomLevel)
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

void lotus::BottomLevelAccelerationStructure::Update(vk::CommandBuffer buffer)
{
    UpdateAccelerationStructure(buffer, geometries, geometry_offsets);
}

lotus::TopLevelAccelerationStructure::TopLevelAccelerationStructure(Engine* _engine, bool _updateable) : AccelerationStructure(_engine, vk::AccelerationStructureTypeKHR::eTopLevel), updateable(_updateable)
{
    instances.reserve(Renderer::max_acceleration_binding_index);
    if (updateable)
        flags |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
}

uint32_t lotus::TopLevelAccelerationStructure::AddInstance(vk::AccelerationStructureInstanceKHR instance)
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
        uint32_t i = engine->renderer.getCurrentImage();
        if (!instance_memory)
        {
            instance_memory = engine->renderer.memory_manager->GetBuffer(instances.size() * sizeof(vk::AccelerationStructureInstanceKHR), vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
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

            vk::WriteDescriptorSet write_info_vertex;
            write_info_vertex.descriptorType = vk::DescriptorType::eStorageBuffer;
            write_info_vertex.dstArrayElement = engine->renderer.static_acceleration_bindings_offset;
            write_info_vertex.dstBinding = 1;
            write_info_vertex.descriptorCount = static_cast<uint32_t>(descriptor_vertex_info.size());
            write_info_vertex.pBufferInfo = descriptor_vertex_info.data();

            vk::WriteDescriptorSet write_info_index;
            write_info_index.descriptorType = vk::DescriptorType::eStorageBuffer;
            write_info_index.dstArrayElement = engine->renderer.static_acceleration_bindings_offset;
            write_info_index.dstBinding = 2;
            write_info_index.descriptorCount = static_cast<uint32_t>(descriptor_index_info.size());
            write_info_index.pBufferInfo = descriptor_index_info.data();

            vk::WriteDescriptorSet write_info_texture;
            write_info_texture.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            write_info_texture.dstArrayElement = engine->renderer.static_acceleration_bindings_offset;
            write_info_texture.dstBinding = 3;
            write_info_texture.descriptorCount = static_cast<uint32_t>(descriptor_texture_info.size());
            write_info_texture.pImageInfo = descriptor_texture_info.data();

            vk::DescriptorBufferInfo mesh_info;
            mesh_info.buffer = engine->renderer.mesh_info_buffer->buffer;
            mesh_info.offset = sizeof(Renderer::MeshInfo) * engine->renderer.max_acceleration_binding_index * i;
            mesh_info.range = sizeof(Renderer::MeshInfo) * engine->renderer.max_acceleration_binding_index;

            vk::WriteDescriptorSet write_info_mesh_info;
            write_info_mesh_info.descriptorType = vk::DescriptorType::eUniformBuffer;
            write_info_mesh_info.dstArrayElement = 0;
            write_info_mesh_info.dstBinding = 4;
            write_info_mesh_info.descriptorCount = 1;
            write_info_mesh_info.pBufferInfo = &mesh_info;

            std::vector<vk::WriteDescriptorSet> writes;
            write_info_vertex.dstSet = *engine->renderer.rtx_descriptor_sets_const[i];
            write_info_index.dstSet = *engine->renderer.rtx_descriptor_sets_const[i];
            write_info_texture.dstSet = *engine->renderer.rtx_descriptor_sets_const[i];
            write_info_as.dstSet = *engine->renderer.rtx_descriptor_sets_const[i];
            write_info_mesh_info.dstSet = *engine->renderer.rtx_descriptor_sets_const[i];
            if (write_info_vertex.descriptorCount > 0)
                writes.push_back(write_info_vertex);
            if (write_info_index.descriptorCount > 0)
                writes.push_back(write_info_index);
            if (write_info_texture.descriptorCount > 0)
                writes.push_back(write_info_texture);
            writes.push_back(write_info_as);
            writes.push_back(write_info_mesh_info);
            engine->renderer.device->updateDescriptorSets(writes, nullptr);
        }
        auto data = instance_memory->map(0, instances.size() * sizeof(vk::AccelerationStructureInstanceKHR), {});
        memcpy(data, instances.data(), instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
        instance_memory->unmap();
        engine->renderer.mesh_info_buffer->flush(Renderer::max_acceleration_binding_index * sizeof(Renderer::MeshInfo) * i, Renderer::max_acceleration_binding_index * sizeof(Renderer::MeshInfo));

        vk::MemoryBarrier barrier;

        barrier.srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eAccelerationStructureReadKHR;
        barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eAccelerationStructureReadKHR;

        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
            {}, barrier, nullptr, nullptr);
        vk::AccelerationStructureGeometryKHR build_geometry{ vk::GeometryTypeKHR::eInstances,
            vk::AccelerationStructureGeometryInstancesDataKHR{ false, engine->renderer.device->getBufferAddressKHR(instance_memory->buffer) } };
        std::vector<vk::AccelerationStructureGeometryKHR> instance_data_vec{ build_geometry };
        vk::AccelerationStructureBuildOffsetInfoKHR build_offset{static_cast<uint32_t>(instances.size()), 0, 0};
        std::vector<vk::AccelerationStructureBuildOffsetInfoKHR> instance_offset_vec{ build_offset };
        BuildAccelerationStructure(command_buffer, instance_data_vec, instance_offset_vec, update);
        dirty = false;
    }
}

void lotus::TopLevelAccelerationStructure::UpdateInstance(uint32_t instance_id, glm::mat3x4 transform)
{
    if (instance_memory)
    {
        memcpy(&instances[instance_id].transform, &transform, sizeof(vk::TransformMatrixKHR));
        dirty = true;
    }
}

void lotus::TopLevelAccelerationStructure::AddBLASResource(Model* model)
{
    uint32_t image = engine->renderer.getCurrentImage();
    uint16_t index = static_cast<uint16_t>(descriptor_vertex_info.size()) + engine->renderer.static_acceleration_bindings_offset;
    for (size_t i = 0; i < model->meshes.size(); ++i)
    {
        auto& mesh = model->meshes[i];
        descriptor_vertex_info.emplace_back(mesh->vertex_buffer->buffer, 0, VK_WHOLE_SIZE);
        descriptor_index_info.emplace_back(mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE);
        descriptor_texture_info.emplace_back(*mesh->texture->sampler, *mesh->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
        engine->renderer.mesh_info_buffer_mapped[image * Renderer::max_acceleration_binding_index + index + i] = { index + (uint32_t)i, index + (uint32_t)i, mesh->specular_exponent, mesh->specular_intensity, glm::vec4{1.f}, model->light_offset };
    }
    model->bottom_level_as->resource_index = index;
}

void lotus::TopLevelAccelerationStructure::AddBLASResource(DeformableEntity* entity)
{
    uint32_t image = engine->renderer.getCurrentImage();
    for (size_t i = 0; i < entity->models.size(); ++i)
    {
        uint16_t index = static_cast<uint16_t>(descriptor_vertex_info.size()) + engine->renderer.static_acceleration_bindings_offset;
        for (size_t j = 0; j < entity->models[i]->meshes.size(); ++j)
        {
            const auto& mesh = entity->models[i]->meshes[j];
            descriptor_vertex_info.emplace_back(entity->animation_component->transformed_geometries[i].vertex_buffers[j][image]->buffer, 0, VK_WHOLE_SIZE);
            descriptor_index_info.emplace_back(mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE);
            descriptor_texture_info.emplace_back(*mesh->texture->sampler, *mesh->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
            engine->renderer.mesh_info_buffer_mapped[image * Renderer::max_acceleration_binding_index + index + j] = { index + (uint32_t)j, index + (uint32_t)j, mesh->specular_exponent, mesh->specular_intensity, glm::vec4{1.f}, entity->models[i]->light_offset };
        }
        entity->animation_component->transformed_geometries[i].bottom_level_as[image]->resource_index = index;
    }
}

void lotus::TopLevelAccelerationStructure::AddBLASResource(Particle* entity)
{
    uint32_t image = engine->renderer.getCurrentImage();
    uint16_t index = static_cast<uint16_t>(descriptor_vertex_info.size()) + engine->renderer.static_acceleration_bindings_offset;
    auto& model = entity->models[0];
    for (size_t i = 0; i < model->meshes.size(); ++i)
    {
        auto& mesh = model->meshes[i];
        descriptor_vertex_info.emplace_back(mesh->vertex_buffer->buffer, 0, VK_WHOLE_SIZE);
        descriptor_index_info.emplace_back(mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE);
        descriptor_texture_info.emplace_back(*mesh->texture->sampler, *mesh->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
        engine->renderer.mesh_info_buffer_mapped[image * Renderer::max_acceleration_binding_index + index + i] = { index + (uint32_t)i, index + (uint32_t)i, mesh->specular_exponent, mesh->specular_intensity, entity->color, model->light_offset };
    }
    *(uint32_t*)(entity->mesh_index_buffer_mapped + (image * engine->renderer.uniform_buffer_align_up(sizeof(uint32_t)))) = index;
    entity->resource_index = index;
}
