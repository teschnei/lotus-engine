#include "raytrace_query.h"

#include "engine/core.h"
#include "engine/game.h"
#include "engine/renderer/acceleration_structure.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    RaytraceQueryer::RaytraceQueryer(Engine* _engine) : engine(_engine)
    {
        if (engine->config->renderer.RaytraceEnabled())
        {
            auto rayquery_shader_module = engine->renderer->getShader("shaders/rayquery.spv");
            auto query_miss_shader_module = engine->renderer->getShader("shaders/query_miss.spv");
            auto query_closest_hit_module = engine->renderer->getShader("shaders/query_closest_hit.spv");

            vk::PipelineShaderStageCreateInfo rayquery_stage_ci;
            rayquery_stage_ci.stage = vk::ShaderStageFlagBits::eRaygenKHR;
            rayquery_stage_ci.module = *rayquery_shader_module;
            rayquery_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo ray_miss_stage_ci;
            ray_miss_stage_ci.stage = vk::ShaderStageFlagBits::eMissKHR;
            ray_miss_stage_ci.module = *query_miss_shader_module;
            ray_miss_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo ray_closest_hit_stage_ci;
            ray_closest_hit_stage_ci.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
            ray_closest_hit_stage_ci.module = *query_closest_hit_module;
            ray_closest_hit_stage_ci.pName = "main";

            constexpr uint32_t shader_raygencount = 1;
            constexpr uint32_t shader_misscount = 1;
            constexpr uint32_t shader_nonhitcount = shader_raygencount + shader_misscount;
            constexpr uint32_t shader_hitcount = RaytracePipeline::shaders_per_group * 6;
            std::vector<vk::PipelineShaderStageCreateInfo> shaders_ci = { rayquery_stage_ci, ray_miss_stage_ci, ray_closest_hit_stage_ci };

            std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shader_group_ci = {
                {
                .type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
                .generalShader = 0,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR
                },
                {
                .type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
                .generalShader = 1,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR
                }
            };

            for (int i = 0; i < shader_hitcount; ++i)
            {
                shader_group_ci.push_back({
                    .type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                    .generalShader = VK_SHADER_UNUSED_KHR,
                    .closestHitShader = 2,
                    .anyHitShader = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR
                });
            }

            vk::DeviceSize shader_stride = engine->renderer->gpu->ray_tracing_properties.shaderGroupHandleSize;
            vk::DeviceSize shader_offset_raygen = 0;
            vk::DeviceSize shader_offset_miss = (((shader_stride * shader_raygencount) / engine->renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment) + 1) * engine->renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment;
            vk::DeviceSize shader_offset_hit = shader_offset_miss + (((shader_stride * shader_misscount) / engine->renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment) + 1) * engine->renderer->gpu->ray_tracing_properties.shaderGroupBaseAlignment;
            vk::DeviceSize sbt_size = (shader_stride * shader_hitcount) + shader_offset_hit;
            shader_binding_table = engine->renderer->gpu->memory_manager->GetBuffer(sbt_size, vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eHostVisible);

            vk::DescriptorSetLayoutBinding acceleration_structure_binding;
            acceleration_structure_binding.binding = 0;
            acceleration_structure_binding.descriptorCount = 1;
            acceleration_structure_binding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
            acceleration_structure_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

            vk::DescriptorSetLayoutBinding input_ubo_binding;
            input_ubo_binding.binding = 1;
            input_ubo_binding.descriptorCount = 1;
            input_ubo_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
            input_ubo_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

            vk::DescriptorSetLayoutBinding output_buffer_binding;
            output_buffer_binding.binding = 2;
            output_buffer_binding.descriptorCount = 1;
            output_buffer_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
            output_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

            std::vector<vk::DescriptorSetLayoutBinding> rtx_bindings
            {
                acceleration_structure_binding,
                input_ubo_binding,
                output_buffer_binding
            };

            vk::DescriptorSetLayoutCreateInfo rtx_layout_info;
            rtx_layout_info.bindingCount = static_cast<uint32_t>(rtx_bindings.size());
            rtx_layout_info.pBindings = rtx_bindings.data();

            rtx_descriptor_layout = engine->renderer->gpu->device->createDescriptorSetLayoutUnique(rtx_layout_info, nullptr);

            std::vector<vk::DescriptorSetLayout> rtx_descriptor_layouts = { *rtx_descriptor_layout};
            vk::PipelineLayoutCreateInfo rtx_pipeline_layout_ci;
            rtx_pipeline_layout_ci.pSetLayouts = rtx_descriptor_layouts.data();
            rtx_pipeline_layout_ci.setLayoutCount = static_cast<uint32_t>(rtx_descriptor_layouts.size());

            rtx_pipeline_layout = engine->renderer->gpu->device->createPipelineLayoutUnique(rtx_pipeline_layout_ci, nullptr);

            vk::RayTracingPipelineCreateInfoKHR rtx_pipeline_ci;
            rtx_pipeline_ci.maxPipelineRayRecursionDepth = 1;
            rtx_pipeline_ci.stageCount = static_cast<uint32_t>(shaders_ci.size());
            rtx_pipeline_ci.pStages = shaders_ci.data();
            rtx_pipeline_ci.groupCount = static_cast<uint32_t>(shader_group_ci.size());
            rtx_pipeline_ci.pGroups = shader_group_ci.data();
            rtx_pipeline_ci.layout = *rtx_pipeline_layout;

            auto result = engine->renderer->gpu->device->createRayTracingPipelineKHRUnique(nullptr, nullptr, rtx_pipeline_ci, nullptr);
            rtx_pipeline = std::move(result.value);

            std::vector<vk::DescriptorPoolSize> pool_sizes_const;
            pool_sizes_const.emplace_back(vk::DescriptorType::eAccelerationStructureKHR, 1);
            pool_sizes_const.emplace_back(vk::DescriptorType::eUniformBuffer, 1);
            pool_sizes_const.emplace_back(vk::DescriptorType::eStorageBuffer, 1);

            vk::DescriptorPoolCreateInfo pool_ci;
            pool_ci.maxSets = 1;
            pool_ci.poolSizeCount = static_cast<uint32_t>(pool_sizes_const.size());
            pool_ci.pPoolSizes = pool_sizes_const.data();
            pool_ci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

            rtx_descriptor_pool = engine->renderer->gpu->device->createDescriptorPoolUnique(pool_ci, nullptr);

            vk::DescriptorSetAllocateInfo set_ci;
            set_ci.descriptorPool = *rtx_descriptor_pool;
            set_ci.descriptorSetCount = 1;
            set_ci.pSetLayouts = &*rtx_descriptor_layout;
            {
                auto sets = engine->renderer->gpu->device->allocateDescriptorSetsUnique(set_ci);
                rtx_descriptor_set = std::move(sets[0]);
            }

            uint8_t* shader_mapped = static_cast<uint8_t*>(shader_binding_table->map(0, sbt_size, {}));

            std::vector<uint8_t> shader_handle_storage((shader_hitcount + shader_nonhitcount) * shader_stride);
            engine->renderer->gpu->device->getRayTracingShaderGroupHandlesKHR(*rtx_pipeline, 0, shader_nonhitcount + shader_hitcount, shader_handle_storage.size(), shader_handle_storage.data());
            for (uint32_t i = 0; i < shader_raygencount; ++i)
            {
                memcpy(shader_mapped + shader_offset_raygen + (i * shader_stride), shader_handle_storage.data() + (i * shader_stride), shader_stride);
            }
            for (uint32_t i = 0; i < shader_misscount; ++i)
            {
                memcpy(shader_mapped + shader_offset_miss + (i * shader_stride), shader_handle_storage.data() + (shader_stride * shader_raygencount) + (i * shader_stride), shader_stride);
            }
            for (uint32_t i = 0; i < shader_hitcount; ++i)
            {
                memcpy(shader_mapped + shader_offset_hit + (i * shader_stride), shader_handle_storage.data() + (shader_stride * shader_nonhitcount) + (i * shader_stride), shader_stride);
            }
            shader_binding_table->unmap();

            raygenSBT = vk::StridedDeviceAddressRegionKHR{ engine->renderer->gpu->device->getBufferAddress({.buffer = shader_binding_table->buffer}) + shader_offset_raygen, shader_stride, shader_stride * shader_raygencount };
            missSBT = vk::StridedDeviceAddressRegionKHR{ engine->renderer->gpu->device->getBufferAddress({.buffer = shader_binding_table->buffer}) + shader_offset_miss, shader_stride, shader_stride * shader_misscount };
            hitSBT = vk::StridedDeviceAddressRegionKHR{ engine->renderer->gpu->device->getBufferAddress({.buffer = shader_binding_table->buffer}) + shader_offset_hit, shader_stride, shader_stride * shader_hitcount };
        }
        raytrace_query_queue = engine->renderer->gpu->device->getQueue(engine->renderer->gpu->graphics_queue_index, 0);
        vk::FenceCreateInfo fence_info;
        fence = engine->renderer->gpu->device->createFenceUnique(fence_info, nullptr);

        vk::CommandPoolCreateInfo pool_info = {};
        pool_info.queueFamilyIndex = engine->renderer->gpu->graphics_queue_index;

        command_pool = engine->renderer->gpu->device->createCommandPoolUnique(pool_info, nullptr);
    }

    Task<float> RaytraceQueryer::query(ObjectFlags object_flags, glm::vec3 origin, glm::vec3 direction, float min, float max)
    {
        auto q = query_queue(object_flags, origin, direction, min, max);
        if (!query_running.test_and_set())
        {
            runQueries();
        }
        co_return co_await q;
    }

    Task<float> RaytraceQueryer::query_queue(ObjectFlags object_flags, glm::vec3 origin, glm::vec3 direction, float min, float max)
    {
        co_return (co_await async_query_queue.wait(RaytraceQuery{object_flags, origin, direction, min, max})).result;
    }

    void RaytraceQueryer::runQueries()
    {
        if (engine->config->renderer.RaytraceEnabled())
        {
            auto tlas = engine->renderer->raytracer->getTLAS(engine->renderer->getPreviousFrame());
            if (engine->game->scene && tlas && tlas->handle != 0)
            {
                auto processing_queries = async_query_queue.getAll();

                if (!processing_queries.empty())
                {
                    size_t input_buffer_size = sizeof(RaytraceInput) * processing_queries.size();
                    size_t output_buffer_size = sizeof(RaytraceOutput) * processing_queries.size();
                    input_buffer = engine->renderer->gpu->memory_manager->GetBuffer(input_buffer_size, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
                    output_buffer = engine->renderer->gpu->memory_manager->GetBuffer(output_buffer_size, vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
                    RaytraceInput* input_mapped = static_cast<RaytraceInput*>(input_buffer->map(0, input_buffer_size, {}));
                    for (size_t i = 0; i < processing_queries.size(); ++i)
                    {
                        const auto& query = processing_queries[i];
                        input_mapped[i].origin = query->data.origin;
                        input_mapped[i].min = query->data.min;
                        input_mapped[i].direction = query->data.direction;
                        input_mapped[i].max = query->data.max;
                        input_mapped[i].flags = static_cast<uint32_t>(query->data.object_flags);
                    }
                    input_buffer->unmap();

                    vk::CommandBufferAllocateInfo alloc_info = {};
                    alloc_info.commandPool = *command_pool;
                    alloc_info.level = vk::CommandBufferLevel::ePrimary;
                    alloc_info.commandBufferCount = 1;

                    auto buffer = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

                    vk::CommandBufferBeginInfo begin_info = {};
                    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

                    buffer[0]->begin(begin_info);
                    buffer[0]->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline);

                    vk::WriteDescriptorSet write_info_as;
                    write_info_as.descriptorCount = 1;
                    write_info_as.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
                    write_info_as.dstBinding = 0;
                    write_info_as.dstArrayElement = 0;
                    write_info_as.dstSet = *rtx_descriptor_set;

                    vk::WriteDescriptorSetAccelerationStructureKHR write_as;
                    write_as.accelerationStructureCount = 1;
                    write_as.pAccelerationStructures = &*tlas->acceleration_structure;
                    write_info_as.pNext = &write_as;

                    vk::DescriptorBufferInfo input_buffer_info;
                    input_buffer_info.buffer = input_buffer->buffer;
                    input_buffer_info.offset = 0;
                    input_buffer_info.range = sizeof(RaytraceInput) * processing_queries.size();

                    vk::WriteDescriptorSet write_info_input;
                    write_info_input.descriptorCount = 1;
                    write_info_input.descriptorType = vk::DescriptorType::eUniformBuffer;
                    write_info_input.dstBinding = 1;
                    write_info_input.dstArrayElement = 0;
                    write_info_input.pBufferInfo = &input_buffer_info;
                    write_info_input.dstSet = *rtx_descriptor_set;

                    vk::DescriptorBufferInfo output_buffer_info;
                    output_buffer_info.buffer = output_buffer->buffer;
                    output_buffer_info.offset = 0;
                    output_buffer_info.range = sizeof(RaytraceOutput) * processing_queries.size();

                    vk::WriteDescriptorSet write_info_output;
                    write_info_output.descriptorCount = 1;
                    write_info_output.descriptorType = vk::DescriptorType::eStorageBuffer;
                    write_info_output.dstBinding = 2;
                    write_info_output.dstArrayElement = 0;
                    write_info_output.pBufferInfo = &output_buffer_info;
                    write_info_output.dstSet = *rtx_descriptor_set;

                    std::vector<vk::WriteDescriptorSet> writes = { write_info_as, write_info_input, write_info_output };
                    engine->renderer->gpu->device->updateDescriptorSets(writes, nullptr);

                    vk::MemoryBarrier2KHR barrier
                    {
                        .srcStageMask = vk::PipelineStageFlagBits2KHR::eAccelerationStructureBuild,
                        .srcAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureWrite,
                        .dstStageMask = vk::PipelineStageFlagBits2KHR::eRayTracingShader,
                        .dstAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureRead
                    };

                    buffer[0]->pipelineBarrier2KHR({
                        .memoryBarrierCount = 1,
                        .pMemoryBarriers = &barrier
                        });

                    buffer[0]->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline_layout, 0, *rtx_descriptor_set, {});
                    buffer[0]->traceRaysKHR(raygenSBT,
                        missSBT,
                        hitSBT,
                        {},
                        processing_queries.size(), 1, 1);
                    buffer[0]->end();
                    vk::SubmitInfo submit_info = {};
                    submit_info.pCommandBuffers = &*buffer[0];
                    submit_info.commandBufferCount = 1;
                    raytrace_query_queue.submit(submit_info, *fence);
                    engine->renderer->gpu->device->waitForFences(*fence, true, std::numeric_limits<uint64_t>::max());
                    engine->renderer->gpu->device->resetFences(*fence);

                    RaytraceOutput* output_mapped = static_cast<RaytraceOutput*>(output_buffer->map(0, output_buffer_size, {}));
                    for (size_t i = 0; i < processing_queries.size(); ++i)
                    {
                        const auto& query = processing_queries[i];
                        query->data.result = output_mapped[i].intersection_dist;
                        query->awaiting.resume();
                    }
                    output_buffer->unmap();
                    query_running.clear();
                    return;
                }
                return;
            }
        }
        for (auto& query : async_query_queue.getAll())
        {
            query->data.result = query->data.max;
            query->awaiting.resume();
        }
        query_running.clear();
    }
}
