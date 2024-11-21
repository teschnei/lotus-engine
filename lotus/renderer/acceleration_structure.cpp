#include "acceleration_structure.h"
#include "lotus/core.h"
#include "lotus/renderer/model.h"
#include "lotus/renderer/vulkan/renderer.h"

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

    auto size_info =
        renderer->gpu->device->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, build_info, max_primitive_counts);

    scratch_memory = renderer->gpu->memory_manager->GetAlignedBuffer(
        size_info.buildScratchSize > size_info.updateScratchSize ? size_info.buildScratchSize : size_info.updateScratchSize,
        renderer->gpu->acceleration_structure_properties.minAccelerationStructureScratchOffsetAlignment,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal);

    object_memory = renderer->gpu->memory_manager->GetBuffer(
        size_info.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::AccelerationStructureCreateInfoKHR info;
    info.type = type;
    info.buffer = object_memory->buffer;
    info.size = size_info.accelerationStructureSize;
    acceleration_structure = renderer->gpu->device->createAccelerationStructureKHRUnique(info, nullptr);
    handle = renderer->gpu->device->getAccelerationStructureAddressKHR({.accelerationStructure = *acceleration_structure});
}

void AccelerationStructure::UpdateAccelerationStructure(vk::CommandBuffer command_buffer, std::span<vk::AccelerationStructureGeometryKHR> geometries,
                                                        std::span<vk::AccelerationStructureBuildRangeInfoKHR> ranges)
{
    BuildAccelerationStructure(command_buffer, geometries, ranges, vk::BuildAccelerationStructureModeKHR::eUpdate);
}

void AccelerationStructure::BuildAccelerationStructure(vk::CommandBuffer command_buffer, std::span<vk::AccelerationStructureGeometryKHR> geometries,
                                                       std::span<vk::AccelerationStructureBuildRangeInfoKHR> ranges, vk::BuildAccelerationStructureModeKHR mode)
{
    vk::BufferMemoryBarrier2KHR barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        .srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR | vk::AccessFlagBits2::eAccelerationStructureReadKHR,
        .dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR | vk::AccessFlagBits2::eAccelerationStructureReadKHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = scratch_memory->buffer,
        .size = VK_WHOLE_SIZE,
    };

    command_buffer.pipelineBarrier2KHR({.bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &barrier});

    vk::AccelerationStructureBuildGeometryInfoKHR build_info{.type = type,
                                                             .flags = flags,
                                                             .mode = mode,
                                                             .srcAccelerationStructure = *acceleration_structure,
                                                             .dstAccelerationStructure = *acceleration_structure,
                                                             .geometryCount = static_cast<uint32_t>(geometries.size()),
                                                             .pGeometries = geometries.data(),
                                                             .scratchData = renderer->gpu->device->getBufferAddress({.buffer = scratch_memory->buffer})};
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> ranges_v = {ranges.data()};

    command_buffer.buildAccelerationStructuresKHR(build_info, ranges_v);
}

void AccelerationStructure::Copy(vk::CommandBuffer command_buffer, AccelerationStructure& target)
{
    command_buffer.copyAccelerationStructureKHR(
        {.src = *acceleration_structure, .dst = *target.acceleration_structure, .mode = vk::CopyAccelerationStructureModeKHR::eClone});
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(Renderer* _renderer, class vk::CommandBuffer command_buffer,
                                                                   std::vector<vk::AccelerationStructureGeometryKHR>&& geometry,
                                                                   std::vector<vk::AccelerationStructureBuildRangeInfoKHR>&& ranges,
                                                                   std::vector<uint32_t>&& primitive_counts, bool updateable, bool compact,
                                                                   Performance performance)
    : AccelerationStructure(_renderer, vk::AccelerationStructureTypeKHR::eBottomLevel)
{
    if (performance == Performance::FastBuild)
        flags |= vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild;
    else if (performance == Performance::FastTrace)
        flags |= vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    if (compact)
        flags |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction;
    if (updateable)
        flags |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
    // TODO: compact
    geometries = std::move(geometry);
    geometry_ranges = std::move(ranges);
    max_primitive_counts = std::move(primitive_counts);
    CreateAccelerationStructure(geometries, max_primitive_counts);
    BuildAccelerationStructure(command_buffer, geometries, geometry_ranges, vk::BuildAccelerationStructureModeKHR::eBuild);
}

void BottomLevelAccelerationStructure::Update(vk::CommandBuffer buffer) { UpdateAccelerationStructure(buffer, geometries, geometry_ranges); }

TopLevelAccelerationStructure::TopLevelAccelerationStructure(Renderer* _renderer, TopLevelAccelerationStructureInstances& _instances, bool _updateable)
    : AccelerationStructure(_renderer, vk::AccelerationStructureTypeKHR::eTopLevel), instances(_instances), updateable(_updateable)
{
    if (updateable)
        flags |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
}

uint32_t TopLevelAccelerationStructure::AddInstance(vk::AccelerationStructureInstanceKHR instance)
{
    uint32_t instanceid = instance_index.fetch_add(1);
    instances.SetInstance(instance, instanceid);
    return instanceid;
}

WorkerTask<> TopLevelAccelerationStructure::Build(Engine* engine)
{
    uint32_t instance_count = instance_index.load();
    // priority: 2
    vk::CommandBufferAllocateInfo alloc_info{
        .commandPool = *renderer->graphics_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };

    auto command_buffers = renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
    auto command_buffer = std::move(command_buffers[0]);

    command_buffer->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    bool update = true;

    if (!instance_memory)
    {
        instance_memory = renderer->gpu->memory_manager->GetBuffer(instance_count * sizeof(vk::AccelerationStructureInstanceKHR),
                                                                   vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                                                                       vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                                                   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        update = false;
    }

    std::vector<vk::AccelerationStructureGeometryKHR> instance_data_vec{
        {.geometryType = vk::GeometryTypeKHR::eInstances,
         .geometry = {.instances = vk::AccelerationStructureGeometryInstancesDataKHR{
                          .arrayOfPointers = false, .data = renderer->gpu->device->getBufferAddress({.buffer = instance_memory->buffer})}}}};
    vk::AccelerationStructureBuildRangeInfoKHR build_range{.primitiveCount = instance_count};
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR> instance_range_vec{build_range};

    if (!update)
    {
        // info.allowsTransforms = true;
        std::vector<uint32_t> max_primitives{{instance_count}};
        CreateAccelerationStructure(instance_data_vec, max_primitives);
    }
    auto data = instance_memory->map(0, instance_count * sizeof(vk::AccelerationStructureInstanceKHR), {});
    memcpy(data, instances.GetData(), instance_count * sizeof(vk::AccelerationStructureInstanceKHR));
    instance_memory->unmap();

    vk::MemoryBarrier2KHR barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        .srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR | vk::AccessFlagBits2::eAccelerationStructureReadKHR,
        .dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR | vk::AccessFlagBits2::eAccelerationStructureReadKHR,
    };

    command_buffer->pipelineBarrier2KHR({.memoryBarrierCount = 1, .pMemoryBarriers = &barrier});

    BuildAccelerationStructure(*command_buffer, instance_data_vec, instance_range_vec,
                               update ? vk::BuildAccelerationStructureModeKHR::eUpdate : vk::BuildAccelerationStructureModeKHR::eBuild);

    command_buffer->end();

    engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
    engine->worker_pool->gpuResource(std::move(command_buffer));
    co_return;
}

void TopLevelAccelerationStructure::TopLevelAccelerationStructureInstances::SetInstance(vk::AccelerationStructureInstanceKHR instance, size_t index)
{
    if (index >= size)
    {
        if (!reallocating.test_and_set())
        {
            ReallocateInstances();
            reallocating_cv.notify_all();
            reallocating.clear();
        }
        else
        {
            std::unique_lock lk(reallocating_mutex);
            reallocating_cv.wait(lk, [this, index] { return index < size; });
        }
    }
    instances[index] = instance;
}

void TopLevelAccelerationStructure::TopLevelAccelerationStructureInstances::ReallocateInstances()
{
    instances.resize(size * 2);
    size *= 2;
}
} // namespace lotus
