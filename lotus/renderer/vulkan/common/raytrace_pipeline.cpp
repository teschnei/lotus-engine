module;

#include <array>
#include <coroutine>
#include <cstring>
#include <format>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

module lotus;

import :renderer.vulkan.pipelines.raytrace;

import :core.engine;
import :renderer.memory;
import :renderer.vulkan.renderer;
import vulkan_hpp;

namespace lotus
{
RaytracePipeline::RaytracePipeline(Renderer* _renderer, std::string raygen, std::span<vk::DescriptorSetLayoutBinding> input_output_descriptor_layout_desc)
    : renderer(_renderer)
{
    resources_descriptor_layout = initializeResourceDescriptorSetLayout(renderer);
    resources_descriptor_pool = initializeResourceDescriptorPool(renderer, *resources_descriptor_layout);
    resources_descriptor_sets = initializeResourceDescriptorSets(renderer, *resources_descriptor_layout, *resources_descriptor_pool);
    input_output_descriptor_layout = initializeInputOutputDescriptorSetLayout(renderer, input_output_descriptor_layout_desc);

    std::array descriptors{*resources_descriptor_layout, *input_output_descriptor_layout, renderer->global_descriptors->getDescriptorLayout()};
    pipeline_layout = initializePipelineLayout(renderer, descriptors);
    pipeline = initializePipeline(renderer, *pipeline_layout, raygen);
    shader_binding_table = initializeSBT(renderer, *pipeline);

    tlas.resize(renderer->getFrameCount());
}

vk::UniqueDescriptorSetLayout RaytracePipeline::initializeResourceDescriptorSetLayout(Renderer* renderer)
{
    std::array descriptors{vk::DescriptorSetLayoutBinding{
        // acceleration structure
        .binding = 0,
        .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR,
    }};

    return renderer->gpu->device->createDescriptorSetLayoutUnique({.bindingCount = descriptors.size(), .pBindings = descriptors.data()});
}

vk::UniqueDescriptorPool RaytracePipeline::initializeResourceDescriptorPool(Renderer* renderer, vk::DescriptorSetLayout layout)
{
    std::array pool_sizes{vk::DescriptorPoolSize{vk::DescriptorType::eAccelerationStructureKHR, renderer->getFrameCount()}};

    return renderer->gpu->device->createDescriptorPoolUnique({
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = renderer->getFrameCount(),
        .poolSizeCount = pool_sizes.size(),
        .pPoolSizes = pool_sizes.data(),
    });
}

std::vector<vk::UniqueDescriptorSet> RaytracePipeline::initializeResourceDescriptorSets(Renderer* renderer, vk::DescriptorSetLayout layout,
                                                                                        vk::DescriptorPool pool)
{
    std::vector<vk::DescriptorSetLayout> layouts(renderer->getFrameCount(), layout);

    return renderer->gpu->device->allocateDescriptorSetsUnique({
        .descriptorPool = pool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
    });
}

vk::UniqueDescriptorSetLayout RaytracePipeline::initializeInputOutputDescriptorSetLayout(Renderer* renderer,
                                                                                         std::span<vk::DescriptorSetLayoutBinding> descriptors)
{
    return renderer->gpu->device->createDescriptorSetLayoutUnique({
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptor,
        .bindingCount = static_cast<uint32_t>(descriptors.size()),
        .pBindings = descriptors.data(),
    });
}

vk::UniquePipelineLayout RaytracePipeline::initializePipelineLayout(Renderer* renderer, std::span<vk::DescriptorSetLayout> layouts)
{
    return renderer->gpu->device->createPipelineLayoutUnique({
        .setLayoutCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
    });
}

vk::UniquePipeline RaytracePipeline::initializePipeline(Renderer* renderer, vk::PipelineLayout pipeline_layout, std::string raygen)
{
    // ray-tracing pipeline
    auto raytrace_module = renderer->getShader(std::format("shaders/raytrace.spv", raygen));
    auto raytrace_internal_module = renderer->getShader(std::format("shaders/{}.spv", raygen));
    auto mmb_module = renderer->getShader(std::format("shaders/raytrace_mmb.spv", raygen));
    auto sk2_module = renderer->getShader(std::format("shaders/raytrace_sk2.spv", raygen));
    auto d3m_module = renderer->getShader(std::format("shaders/raytrace_d3m.spv", raygen));
    auto water_module = renderer->getShader(std::format("shaders/raytrace_water.spv", raygen));

    std::array shaders{vk::PipelineShaderStageCreateInfo{// raygen
                                                         .stage = vk::ShaderStageFlagBits::eRaygenKHR,
                                                         .module = *raytrace_internal_module,
                                                         .pName = "Raygen"},
                       vk::PipelineShaderStageCreateInfo{// miss
                                                         .stage = vk::ShaderStageFlagBits::eMissKHR,
                                                         .module = *raytrace_module,
                                                         .pName = "Miss"},
                       vk::PipelineShaderStageCreateInfo{// miss (GI)
                                                         .stage = vk::ShaderStageFlagBits::eMissKHR,
                                                         .module = *raytrace_internal_module,
                                                         .pName = "MissGI"},
                       vk::PipelineShaderStageCreateInfo{// miss (shadow)
                                                         .stage = vk::ShaderStageFlagBits::eMissKHR,
                                                         .module = *raytrace_internal_module,
                                                         .pName = "MissShadow"},
                       vk::PipelineShaderStageCreateInfo{// closest hit
                                                         .stage = vk::ShaderStageFlagBits::eClosestHitKHR,
                                                         .module = *sk2_module,
                                                         .pName = "ClosestHit"},
                       vk::PipelineShaderStageCreateInfo{// colour hit
                                                         .stage = vk::ShaderStageFlagBits::eAnyHitKHR,
                                                         .module = *sk2_module,
                                                         .pName = "AnyHit"},
                       vk::PipelineShaderStageCreateInfo{// landscape closest hit
                                                         .stage = vk::ShaderStageFlagBits::eClosestHitKHR,
                                                         .module = *mmb_module,
                                                         .pName = "ClosestHit"},
                       vk::PipelineShaderStageCreateInfo{// landscape colour hit
                                                         .stage = vk::ShaderStageFlagBits::eAnyHitKHR,
                                                         .module = *mmb_module,
                                                         .pName = "AnyHit"},
                       vk::PipelineShaderStageCreateInfo{// particle closest hit
                                                         .stage = vk::ShaderStageFlagBits::eClosestHitKHR,
                                                         .module = *d3m_module,
                                                         .pName = "ClosestHit"},
                       vk::PipelineShaderStageCreateInfo{// particle colour hit
                                                         .stage = vk::ShaderStageFlagBits::eAnyHitKHR,
                                                         .module = *d3m_module,
                                                         .pName = "AnyHit"},
                       vk::PipelineShaderStageCreateInfo{// particle shadow colour hit
                                                         .stage = vk::ShaderStageFlagBits::eAnyHitKHR,
                                                         .module = *d3m_module,
                                                         .pName = "AnyHitShadow"},
                       vk::PipelineShaderStageCreateInfo{// particle closest hit (AABB)
                                                         .stage = vk::ShaderStageFlagBits::eClosestHitKHR,
                                                         .module = *d3m_module,
                                                         .pName = "ClosestHitAABB"},
                       vk::PipelineShaderStageCreateInfo{// particle colour hit (AABB)
                                                         .stage = vk::ShaderStageFlagBits::eAnyHitKHR,
                                                         .module = *d3m_module,
                                                         .pName = "AnyHitAABB"},
                       vk::PipelineShaderStageCreateInfo{// particle shadow colour hit (AABB)
                                                         .stage = vk::ShaderStageFlagBits::eAnyHitKHR,
                                                         .module = *d3m_module,
                                                         .pName = "AnyHitShadowAABB"},
                       vk::PipelineShaderStageCreateInfo{// particle intersection
                                                         .stage = vk::ShaderStageFlagBits::eIntersectionKHR,
                                                         .module = *d3m_module,
                                                         .pName = "Intersection"},
                       vk::PipelineShaderStageCreateInfo{// water closest hit
                                                         .stage = vk::ShaderStageFlagBits::eClosestHitKHR,
                                                         .module = *water_module,
                                                         .pName = "ClosestHit"}};

    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shader_group_ci = {{.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
                                                                            .generalShader = 0,
                                                                            .closestHitShader = vk::ShaderUnusedKHR,
                                                                            .anyHitShader = vk::ShaderUnusedKHR,
                                                                            .intersectionShader = vk::ShaderUnusedKHR},
                                                                           {.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
                                                                            .generalShader = 1,
                                                                            .closestHitShader = vk::ShaderUnusedKHR,
                                                                            .anyHitShader = vk::ShaderUnusedKHR,
                                                                            .intersectionShader = vk::ShaderUnusedKHR},
                                                                           {.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
                                                                            .generalShader = 2,
                                                                            .closestHitShader = vk::ShaderUnusedKHR,
                                                                            .anyHitShader = vk::ShaderUnusedKHR,
                                                                            .intersectionShader = vk::ShaderUnusedKHR},
                                                                           {.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
                                                                            .generalShader = 3,
                                                                            .closestHitShader = vk::ShaderUnusedKHR,
                                                                            .anyHitShader = vk::ShaderUnusedKHR,
                                                                            .intersectionShader = vk::ShaderUnusedKHR}};
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = vk::ShaderUnusedKHR,
                                   .closestHitShader = 4,
                                   .anyHitShader = 5,
                                   .intersectionShader = vk::ShaderUnusedKHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = vk::ShaderUnusedKHR,
                                   .closestHitShader = vk::ShaderUnusedKHR,
                                   .anyHitShader = 5,
                                   .intersectionShader = vk::ShaderUnusedKHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = vk::ShaderUnusedKHR,
                                   .closestHitShader = 6,
                                   .anyHitShader = 7,
                                   .intersectionShader = vk::ShaderUnusedKHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = vk::ShaderUnusedKHR,
                                   .closestHitShader = vk::ShaderUnusedKHR,
                                   .anyHitShader = 7,
                                   .intersectionShader = vk::ShaderUnusedKHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = vk::ShaderUnusedKHR,
                                   .closestHitShader = 8,
                                   .anyHitShader = 9,
                                   .intersectionShader = vk::ShaderUnusedKHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = vk::ShaderUnusedKHR,
                                   .closestHitShader = vk::ShaderUnusedKHR,
                                   .anyHitShader = 10,
                                   .intersectionShader = vk::ShaderUnusedKHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup,
                                   .generalShader = vk::ShaderUnusedKHR,
                                   .closestHitShader = 11,
                                   .anyHitShader = 12,
                                   .intersectionShader = 14});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup,
                                   .generalShader = vk::ShaderUnusedKHR,
                                   .closestHitShader = vk::ShaderUnusedKHR,
                                   .anyHitShader = 13,
                                   .intersectionShader = 14});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = vk::ShaderUnusedKHR,
                                   .closestHitShader = 15,
                                   .anyHitShader = vk::ShaderUnusedKHR,
                                   .intersectionShader = vk::ShaderUnusedKHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = vk::ShaderUnusedKHR,
                                   .closestHitShader = vk::ShaderUnusedKHR,
                                   .anyHitShader = vk::ShaderUnusedKHR,
                                   .intersectionShader = vk::ShaderUnusedKHR});
    }

    return renderer->gpu->device
        ->createRayTracingPipelineKHRUnique(nullptr, nullptr,
                                            {.stageCount = static_cast<uint32_t>(shaders.size()),
                                             .pStages = shaders.data(),
                                             .groupCount = static_cast<uint32_t>(shader_group_ci.size()),
                                             .pGroups = shader_group_ci.data(),
                                             .maxPipelineRayRecursionDepth = 1,
                                             .layout = pipeline_layout})
        .value;
}

RaytracePipeline::SBT RaytracePipeline::initializeSBT(Renderer* renderer, vk::Pipeline pipeline)
{
    std::unique_ptr<Buffer> shader_binding_table;
    constexpr uint32_t shader_raygencount = 1;
    constexpr uint32_t shader_misscount = 3;
    constexpr uint32_t shader_nonhitcount = shader_raygencount + shader_misscount;
    constexpr uint32_t shader_hitcount = shaders_per_group * 10;

    vk::DeviceSize shader_handle_size = renderer->gpu->ray_tracing_properties.shaderGroupHandleSize;
    vk::DeviceSize nonhit_shader_stride = shader_handle_size;
    vk::DeviceSize hit_shader_stride = nonhit_shader_stride;
    vk::DeviceSize shader_offset_raygen = 0;
    vk::DeviceSize shader_offset_miss = (((nonhit_shader_stride * shader_raygencount) / renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment) + 1) *
                                        renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment;
    vk::DeviceSize shader_offset_hit =
        shader_offset_miss + (((nonhit_shader_stride * shader_misscount) / renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment) + 1) *
                                 renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment;
    vk::DeviceSize sbt_size = (hit_shader_stride * shader_hitcount) + shader_offset_hit;
    shader_binding_table = renderer->gpu->memory_manager->GetAlignedBuffer(
        sbt_size, renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment,
        vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eHostVisible);

    uint8_t* shader_mapped = static_cast<uint8_t*>(shader_binding_table->map(0, sbt_size, {}));

    std::vector<uint8_t> shader_handle_storage((shader_hitcount + shader_nonhitcount) * shader_handle_size);
    renderer->gpu->device->getRayTracingShaderGroupHandlesKHR(pipeline, 0, shader_nonhitcount + shader_hitcount, shader_handle_storage.size(),
                                                              shader_handle_storage.data());
    for (uint32_t i = 0; i < shader_raygencount; ++i)
    {
        memcpy(shader_mapped + shader_offset_raygen + (i * nonhit_shader_stride), shader_handle_storage.data() + (i * shader_handle_size), shader_handle_size);
    }
    for (uint32_t i = 0; i < shader_misscount; ++i)
    {
        memcpy(shader_mapped + shader_offset_miss + (i * nonhit_shader_stride),
               shader_handle_storage.data() + (shader_handle_size * shader_raygencount) + (i * shader_handle_size), shader_handle_size);
    }
    for (uint32_t i = 0; i < shader_hitcount; ++i)
    {
        memcpy(shader_mapped + shader_offset_hit + (i * hit_shader_stride),
               shader_handle_storage.data() + (shader_handle_size * shader_nonhitcount) + (i * shader_handle_size), shader_handle_size);
    }
    shader_binding_table->unmap();

    auto buffer = shader_binding_table->buffer;

    return {.buffer = std::move(shader_binding_table),
            .raygen = {.deviceAddress = renderer->gpu->device->getBufferAddress({.buffer = buffer}) + shader_offset_raygen,
                       .stride = nonhit_shader_stride,
                       .size = nonhit_shader_stride * shader_raygencount},
            .miss = {.deviceAddress = renderer->gpu->device->getBufferAddress({.buffer = buffer}) + shader_offset_miss,
                     .stride = nonhit_shader_stride,
                     .size = nonhit_shader_stride * shader_misscount},
            .hit = {.deviceAddress = renderer->gpu->device->getBufferAddress({.buffer = buffer}) + shader_offset_hit,
                    .stride = hit_shader_stride,
                    .size = hit_shader_stride * shader_hitcount}};
}

void RaytracePipeline::prepareNextFrame()
{
    renderer->engine->worker_pool->gpuResource(std::move(tlas[renderer->getCurrentFrame()]));
    tlas[renderer->getCurrentFrame()] = std::make_unique<TopLevelAccelerationStructure>(renderer, tlas_instances, false);
}

Task<> RaytracePipeline::prepareFrame(Engine* engine) { co_await tlas[renderer->getCurrentFrame()]->Build(engine); }

TopLevelAccelerationStructure* RaytracePipeline::getTLAS(uint32_t image) const
{
    if (image < tlas.size())
        return tlas[image].get();
    return nullptr;
}

vk::UniqueCommandBuffer RaytracePipeline::getCommandBuffer(std::span<vk::WriteDescriptorSet> input_output_descriptors,
                                                           std::span<vk::ImageMemoryBarrier2> before_barriers,
                                                           std::span<vk::ImageMemoryBarrier2> after_barriers)
{
    auto frame = renderer->getCurrentFrame();
    auto buffer = renderer->gpu->device->allocateCommandBuffersUnique(
        {.commandPool = *renderer->command_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1});

    buffer[0]->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    buffer[0]->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *pipeline);

    vk::WriteDescriptorSetAccelerationStructureKHR write_as{.accelerationStructureCount = 1, .pAccelerationStructures = &*tlas[frame]->acceleration_structure};

    vk::WriteDescriptorSet write_info_as{
        .pNext = &write_as,
        .dstSet = *resources_descriptor_sets[frame],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
    };

    renderer->gpu->device->updateDescriptorSets({write_info_as}, nullptr);

    buffer[0]->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *pipeline_layout, 0, *resources_descriptor_sets[frame], {});
    buffer[0]->pushDescriptorSet(vk::PipelineBindPoint::eRayTracingKHR, *pipeline_layout, 1, input_output_descriptors);
    buffer[0]->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *pipeline_layout, 2, renderer->global_descriptors->getDescriptorSet(), {});

    if (!before_barriers.empty())
    {
        buffer[0]->pipelineBarrier2(
            {.imageMemoryBarrierCount = static_cast<uint32_t>(before_barriers.size()), .pImageMemoryBarriers = before_barriers.data()});
    }

    vk::MemoryBarrier2 tlas_barrier{.srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
                                    .srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
                                    .dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                                    .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR};

    buffer[0]->pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &tlas_barrier});

    buffer[0]->traceRaysKHR(shader_binding_table.raygen, shader_binding_table.miss, shader_binding_table.hit, {}, renderer->swapchain->extent.width,
                            renderer->swapchain->extent.height, 1);

    if (!after_barriers.empty())
    {
        buffer[0]->pipelineBarrier2(
            {.imageMemoryBarrierCount = static_cast<uint32_t>(after_barriers.size()), .pImageMemoryBarriers = after_barriers.data()});
    }

    buffer[0]->end();

    return std::move(buffer[0]);
}
} // namespace lotus
