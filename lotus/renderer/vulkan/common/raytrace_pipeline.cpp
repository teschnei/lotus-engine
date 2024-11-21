#include "raytrace_pipeline.h"

#include <format>
#include <ranges>

#include "lotus/core.h"
#include "lotus/renderer/vulkan/renderer.h"

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
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
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
    auto raygen_shader_module = renderer->getShader(std::format("shaders/{}", raygen));
    auto miss_shader_module = renderer->getShader("shaders/miss.spv");
    auto miss_gi_shader_module = renderer->getShader("shaders/miss_gi.spv");
    auto shadow_miss_shader_module = renderer->getShader("shaders/shadow_miss.spv");
    auto closest_hit_shader_module = renderer->getShader("shaders/closesthit.spv");
    auto color_hit_shader_module = renderer->getShader("shaders/color_hit.spv");
    auto landscape_closest_hit_shader_module = renderer->getShader("shaders/mmb_closest_hit.spv");
    auto landscape_color_hit_shader_module = renderer->getShader("shaders/mmb_color_hit.spv");
    auto particle_closest_hit_shader_module = renderer->getShader("shaders/particle_closest_hit.spv");
    auto particle_color_hit_shader_module = renderer->getShader("shaders/particle_color_hit.spv");
    auto particle_shadow_color_hit_shader_module = renderer->getShader("shaders/particle_shadow_color_hit.spv");
    auto particle_closest_hit_aabb_shader_module = renderer->getShader("shaders/particle_closest_hit_aabb.spv");
    auto particle_color_hit_aabb_shader_module = renderer->getShader("shaders/particle_color_hit_aabb.spv");
    auto particle_shadow_color_hit_aabb_shader_module = renderer->getShader("shaders/particle_shadow_color_hit_aabb.spv");
    auto particle_intersection_shader_module = renderer->getShader("shaders/particle_intersection.spv");
    auto water_closest_hit_shader_module = renderer->getShader("shaders/water_closest_hit.spv");

    std::array shaders{vk::PipelineShaderStageCreateInfo{// raygen
                                                         .stage = vk::ShaderStageFlagBits::eRaygenKHR,
                                                         .module = *raygen_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// miss
                                                         .stage = vk::ShaderStageFlagBits::eMissKHR,
                                                         .module = *miss_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// miss (GI)
                                                         .stage = vk::ShaderStageFlagBits::eMissKHR,
                                                         .module = *miss_gi_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// miss (shadow)
                                                         .stage = vk::ShaderStageFlagBits::eMissKHR,
                                                         .module = *shadow_miss_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// closest hit
                                                         .stage = vk::ShaderStageFlagBits::eClosestHitKHR,
                                                         .module = *closest_hit_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// colour hit
                                                         .stage = vk::ShaderStageFlagBits::eAnyHitKHR,
                                                         .module = *color_hit_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// landscape closest hit
                                                         .stage = vk::ShaderStageFlagBits::eClosestHitKHR,
                                                         .module = *landscape_closest_hit_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// landscape colour hit
                                                         .stage = vk::ShaderStageFlagBits::eAnyHitKHR,
                                                         .module = *landscape_color_hit_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// particle closest hit
                                                         .stage = vk::ShaderStageFlagBits::eClosestHitKHR,
                                                         .module = *particle_closest_hit_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// particle colour hit
                                                         .stage = vk::ShaderStageFlagBits::eAnyHitKHR,
                                                         .module = *particle_color_hit_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// particle shadow colour hit
                                                         .stage = vk::ShaderStageFlagBits::eAnyHitKHR,
                                                         .module = *particle_shadow_color_hit_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// particle closest hit (AABB)
                                                         .stage = vk::ShaderStageFlagBits::eClosestHitKHR,
                                                         .module = *particle_closest_hit_aabb_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// particle colour hit (AABB)
                                                         .stage = vk::ShaderStageFlagBits::eAnyHitKHR,
                                                         .module = *particle_color_hit_aabb_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// particle shadow colour hit (AABB)
                                                         .stage = vk::ShaderStageFlagBits::eAnyHitKHR,
                                                         .module = *particle_shadow_color_hit_aabb_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// particle intersection
                                                         .stage = vk::ShaderStageFlagBits::eIntersectionKHR,
                                                         .module = *particle_intersection_shader_module,
                                                         .pName = "main"},
                       vk::PipelineShaderStageCreateInfo{// water closest hit
                                                         .stage = vk::ShaderStageFlagBits::eClosestHitKHR,
                                                         .module = *water_closest_hit_shader_module,
                                                         .pName = "main"}};

    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shader_group_ci = {{.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
                                                                            .generalShader = 0,
                                                                            .closestHitShader = VK_SHADER_UNUSED_KHR,
                                                                            .anyHitShader = VK_SHADER_UNUSED_KHR,
                                                                            .intersectionShader = VK_SHADER_UNUSED_KHR},
                                                                           {.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
                                                                            .generalShader = 1,
                                                                            .closestHitShader = VK_SHADER_UNUSED_KHR,
                                                                            .anyHitShader = VK_SHADER_UNUSED_KHR,
                                                                            .intersectionShader = VK_SHADER_UNUSED_KHR},
                                                                           {.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
                                                                            .generalShader = 2,
                                                                            .closestHitShader = VK_SHADER_UNUSED_KHR,
                                                                            .anyHitShader = VK_SHADER_UNUSED_KHR,
                                                                            .intersectionShader = VK_SHADER_UNUSED_KHR},
                                                                           {.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
                                                                            .generalShader = 3,
                                                                            .closestHitShader = VK_SHADER_UNUSED_KHR,
                                                                            .anyHitShader = VK_SHADER_UNUSED_KHR,
                                                                            .intersectionShader = VK_SHADER_UNUSED_KHR}};
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = VK_SHADER_UNUSED_KHR,
                                   .closestHitShader = 4,
                                   .anyHitShader = 5,
                                   .intersectionShader = VK_SHADER_UNUSED_KHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = VK_SHADER_UNUSED_KHR,
                                   .closestHitShader = VK_SHADER_UNUSED_KHR,
                                   .anyHitShader = 5,
                                   .intersectionShader = VK_SHADER_UNUSED_KHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = VK_SHADER_UNUSED_KHR,
                                   .closestHitShader = 6,
                                   .anyHitShader = 7,
                                   .intersectionShader = VK_SHADER_UNUSED_KHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = VK_SHADER_UNUSED_KHR,
                                   .closestHitShader = VK_SHADER_UNUSED_KHR,
                                   .anyHitShader = 7,
                                   .intersectionShader = VK_SHADER_UNUSED_KHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = VK_SHADER_UNUSED_KHR,
                                   .closestHitShader = 8,
                                   .anyHitShader = 9,
                                   .intersectionShader = VK_SHADER_UNUSED_KHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = VK_SHADER_UNUSED_KHR,
                                   .closestHitShader = VK_SHADER_UNUSED_KHR,
                                   .anyHitShader = 10,
                                   .intersectionShader = VK_SHADER_UNUSED_KHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup,
                                   .generalShader = VK_SHADER_UNUSED_KHR,
                                   .closestHitShader = 11,
                                   .anyHitShader = 12,
                                   .intersectionShader = 14});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup,
                                   .generalShader = VK_SHADER_UNUSED_KHR,
                                   .closestHitShader = VK_SHADER_UNUSED_KHR,
                                   .anyHitShader = 13,
                                   .intersectionShader = 14});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = VK_SHADER_UNUSED_KHR,
                                   .closestHitShader = 15,
                                   .anyHitShader = VK_SHADER_UNUSED_KHR,
                                   .intersectionShader = VK_SHADER_UNUSED_KHR});
    }
    for (int i = 0; i < shaders_per_group; ++i)
    {
        shader_group_ci.push_back({.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                                   .generalShader = VK_SHADER_UNUSED_KHR,
                                   .closestHitShader = VK_SHADER_UNUSED_KHR,
                                   .anyHitShader = VK_SHADER_UNUSED_KHR,
                                   .intersectionShader = VK_SHADER_UNUSED_KHR});
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
                                                           std::span<vk::ImageMemoryBarrier2KHR> before_barriers,
                                                           std::span<vk::ImageMemoryBarrier2KHR> after_barriers)
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
    buffer[0]->pushDescriptorSetKHR(vk::PipelineBindPoint::eRayTracingKHR, *pipeline_layout, 1, input_output_descriptors);
    buffer[0]->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *pipeline_layout, 2, renderer->global_descriptors->getDescriptorSet(), {});

    if (!before_barriers.empty())
    {
        buffer[0]->pipelineBarrier2KHR(
            {.imageMemoryBarrierCount = static_cast<uint32_t>(before_barriers.size()), .pImageMemoryBarriers = before_barriers.data()});
    }

    vk::MemoryBarrier2 tlas_barrier{.srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
                                    .srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
                                    .dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                                    .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR};

    buffer[0]->pipelineBarrier2KHR({.memoryBarrierCount = 1, .pMemoryBarriers = &tlas_barrier});

    buffer[0]->traceRaysKHR(shader_binding_table.raygen, shader_binding_table.miss, shader_binding_table.hit, {}, renderer->swapchain->extent.width,
                            renderer->swapchain->extent.height, 1);

    if (!after_barriers.empty())
    {
        buffer[0]->pipelineBarrier2KHR(
            {.imageMemoryBarrierCount = static_cast<uint32_t>(after_barriers.size()), .pImageMemoryBarriers = after_barriers.data()});
    }

    buffer[0]->end();

    return std::move(buffer[0]);
}
} // namespace lotus
