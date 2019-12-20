#include "raytrace_query.h"

#include "engine/core.h"
#include "engine/game.h"
#include "engine/renderer/acceleration_structure.h"

namespace lotus
{
    Raytracer::Raytracer(Engine* _engine) : engine(_engine)
    {
        if (engine->renderer.RTXEnabled())
        {
            auto rayquery_shader_module = engine->renderer.getShader("shaders/rayquery.spv");
            auto query_miss_shader_module = engine->renderer.getShader("shaders/query_miss.spv");
            auto query_closest_hit_module = engine->renderer.getShader("shaders/query_closest_hit.spv");

            vk::PipelineShaderStageCreateInfo rayquery_stage_ci;
            rayquery_stage_ci.stage = vk::ShaderStageFlagBits::eRaygenNV;
            rayquery_stage_ci.module = *rayquery_shader_module;
            rayquery_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo ray_miss_stage_ci;
            ray_miss_stage_ci.stage = vk::ShaderStageFlagBits::eMissNV;
            ray_miss_stage_ci.module = *query_miss_shader_module;
            ray_miss_stage_ci.pName = "main";

            vk::PipelineShaderStageCreateInfo ray_closest_hit_stage_ci;
            ray_closest_hit_stage_ci.stage = vk::ShaderStageFlagBits::eClosestHitNV;
            ray_closest_hit_stage_ci.module = *query_closest_hit_module;
            ray_closest_hit_stage_ci.pName = "main";

            std::vector<vk::PipelineShaderStageCreateInfo> shaders_ci = { rayquery_stage_ci, ray_miss_stage_ci, ray_closest_hit_stage_ci };

            std::vector<vk::RayTracingShaderGroupCreateInfoNV> shader_group_ci = {
                {
                vk::RayTracingShaderGroupTypeNV::eGeneral,
                0,
                VK_SHADER_UNUSED_NV,
                VK_SHADER_UNUSED_NV,
                VK_SHADER_UNUSED_NV
                },
                {
                vk::RayTracingShaderGroupTypeNV::eGeneral,
                1,
                VK_SHADER_UNUSED_NV,
                VK_SHADER_UNUSED_NV,
                VK_SHADER_UNUSED_NV
                }
            };

            for (int i = 0; i < 64; ++i)
            {
                shader_group_ci.emplace_back(
                    vk::RayTracingShaderGroupTypeNV::eTrianglesHitGroup,
                    VK_SHADER_UNUSED_NV,
                    2,
                    VK_SHADER_UNUSED_NV,
                    VK_SHADER_UNUSED_NV
                );
            }

            constexpr uint32_t shader_nonhitcount = 2;
            constexpr uint32_t shader_hitcount = 64;
            auto shader_size = engine->renderer.ray_tracing_properties.shaderGroupHandleSize;
            size_t shader_stride = shader_size;
            vk::DeviceSize sbt_size = shader_stride * shader_hitcount + shader_nonhitcount * shader_size;
            shader_binding_table = engine->renderer.memory_manager->GetBuffer(sbt_size, vk::BufferUsageFlagBits::eRayTracingNV, vk::MemoryPropertyFlagBits::eHostVisible);

            vk::DescriptorSetLayoutBinding acceleration_structure_binding;
            acceleration_structure_binding.binding = 0;
            acceleration_structure_binding.descriptorCount = 1;
            acceleration_structure_binding.descriptorType = vk::DescriptorType::eAccelerationStructureNV;
            acceleration_structure_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV;

            vk::DescriptorSetLayoutBinding input_ubo_binding;
            input_ubo_binding.binding = 1;
            input_ubo_binding.descriptorCount = 1;
            input_ubo_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
            input_ubo_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV;

            vk::DescriptorSetLayoutBinding output_buffer_binding;
            output_buffer_binding.binding = 2;
            output_buffer_binding.descriptorCount = 1;
            output_buffer_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
            output_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenNV;

            std::vector<vk::DescriptorSetLayoutBinding> rtx_bindings
            {
                acceleration_structure_binding,
                input_ubo_binding,
                output_buffer_binding
            };

            vk::DescriptorSetLayoutCreateInfo rtx_layout_info;
            rtx_layout_info.bindingCount = static_cast<uint32_t>(rtx_bindings.size());
            rtx_layout_info.pBindings = rtx_bindings.data();

            rtx_descriptor_layout = engine->renderer.device->createDescriptorSetLayoutUnique(rtx_layout_info, nullptr, engine->renderer.dispatch);

            std::vector<vk::DescriptorSetLayout> rtx_descriptor_layouts = { *rtx_descriptor_layout};
            vk::PipelineLayoutCreateInfo rtx_pipeline_layout_ci;
            rtx_pipeline_layout_ci.pSetLayouts = rtx_descriptor_layouts.data();
            rtx_pipeline_layout_ci.setLayoutCount = static_cast<uint32_t>(rtx_descriptor_layouts.size());

            rtx_pipeline_layout = engine->renderer.device->createPipelineLayoutUnique(rtx_pipeline_layout_ci, nullptr, engine->renderer.dispatch);

            vk::RayTracingPipelineCreateInfoNV rtx_pipeline_ci;
            rtx_pipeline_ci.maxRecursionDepth = 1;
            rtx_pipeline_ci.stageCount = static_cast<uint32_t>(shaders_ci.size());
            rtx_pipeline_ci.pStages = shaders_ci.data();
            rtx_pipeline_ci.groupCount = static_cast<uint32_t>(shader_group_ci.size());
            rtx_pipeline_ci.pGroups = shader_group_ci.data();
            rtx_pipeline_ci.layout = *rtx_pipeline_layout;

            rtx_pipeline = engine->renderer.device->createRayTracingPipelineNVUnique(nullptr, rtx_pipeline_ci, nullptr, engine->renderer.dispatch);

            std::vector<vk::DescriptorPoolSize> pool_sizes_const;
            pool_sizes_const.emplace_back(vk::DescriptorType::eAccelerationStructureNV, 1);
            pool_sizes_const.emplace_back(vk::DescriptorType::eUniformBuffer, 1);
            pool_sizes_const.emplace_back(vk::DescriptorType::eStorageBuffer, 1);

            vk::DescriptorPoolCreateInfo pool_ci;
            pool_ci.maxSets = 1;
            pool_ci.poolSizeCount = static_cast<uint32_t>(pool_sizes_const.size());
            pool_ci.pPoolSizes = pool_sizes_const.data();

            rtx_descriptor_pool = engine->renderer.device->createDescriptorPoolUnique(pool_ci, nullptr, engine->renderer.dispatch);

            vk::DescriptorSetAllocateInfo set_ci;
            set_ci.descriptorPool = *rtx_descriptor_pool;
            set_ci.descriptorSetCount = 1;
            set_ci.pSetLayouts = &*rtx_descriptor_layout;
            {
                auto sets = engine->renderer.device->allocateDescriptorSetsUnique<std::allocator<vk::UniqueHandle<vk::DescriptorSet, vk::DispatchLoaderDynamic>>>(set_ci, engine->renderer.dispatch);
                rtx_descriptor_set = std::move(sets[0]);
            }

            uint8_t* shader_mapped = static_cast<uint8_t*>(shader_binding_table->map(0, sbt_size, {}));

            std::vector<uint8_t> shader_handle_storage((shader_hitcount + shader_nonhitcount) * shader_size);
            engine->renderer.device->getRayTracingShaderGroupHandlesNV(*rtx_pipeline, 0, shader_nonhitcount + shader_hitcount, shader_handle_storage.size(), shader_handle_storage.data(), engine->renderer.dispatch);
            for (uint32_t i = 0; i < shader_nonhitcount; ++i)
            {
                memcpy(shader_mapped + (i * shader_size), shader_handle_storage.data() + (i * shader_size), shader_size);
            }
            shader_mapped += shader_size * shader_nonhitcount;

            for (uint32_t i = 0; i < shader_hitcount; ++i)
            {
                memcpy(shader_mapped + (i * shader_stride), shader_handle_storage.data() + (shader_size * shader_nonhitcount) + (i * shader_size), shader_size);
            }
            shader_binding_table->unmap();

        }
        auto [graphics_queue_idx, present_queue_idx, compute_queue_idx] = engine->renderer.getQueueFamilies(engine->renderer.physical_device);
        raytrace_query_queue = engine->renderer.device->getQueue(compute_queue_idx.value(), 1);
        vk::FenceCreateInfo fence_info;
        fence = engine->renderer.device->createFenceUnique(fence_info, nullptr, engine->renderer.dispatch);

        vk::CommandPoolCreateInfo pool_info = {};
        pool_info.queueFamilyIndex = compute_queue_idx.value();

        command_pool = engine->renderer.device->createCommandPoolUnique(pool_info, nullptr, engine->renderer.dispatch);
    }

    void Raytracer::query(ObjectFlags object_flags, glm::vec3 origin, glm::vec3 direction, float min, float max, std::function<void(float)> callback)
    {
        queries.emplace_back(object_flags, origin, direction, min, max, callback);
    }

    void Raytracer::runQueries(uint32_t image)
    {
        if (engine->game->scene->top_level_as[image])
        {
            size_t input_buffer_size = sizeof(RaytraceInput) * queries.size();
            size_t output_buffer_size = sizeof(RaytraceOutput) * queries.size();
            input_buffer = engine->renderer.memory_manager->GetBuffer(input_buffer_size, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            output_buffer = engine->renderer.memory_manager->GetBuffer(output_buffer_size, vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            RaytraceInput* input_mapped = static_cast<RaytraceInput*>(input_buffer->map(0, input_buffer_size, {}));
            for (size_t i = 0; i < queries.size(); ++i)
            {
                const auto& query = queries[i];
                input_mapped[i].origin = query.origin;
                input_mapped[i].min = query.min;
                input_mapped[i].direction = query.direction;
                input_mapped[i].max = query.max;
                input_mapped[i].flags = static_cast<uint32_t>(query.object_flags);
            }
            input_buffer->unmap();

            vk::CommandBufferAllocateInfo alloc_info = {};
            alloc_info.commandPool = *command_pool;
            alloc_info.level = vk::CommandBufferLevel::ePrimary;
            alloc_info.commandBufferCount = 1;

            auto buffer = engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, engine->renderer.dispatch);

            vk::CommandBufferBeginInfo begin_info = {};
            begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

            buffer[0]->begin(begin_info, engine->renderer.dispatch);
            buffer[0]->bindPipeline(vk::PipelineBindPoint::eRayTracingNV, *rtx_pipeline, engine->renderer.dispatch);

            vk::WriteDescriptorSet write_info_as;
            write_info_as.descriptorCount = 1;
            write_info_as.descriptorType = vk::DescriptorType::eAccelerationStructureNV;
            write_info_as.dstBinding = 0;
            write_info_as.dstArrayElement = 0;
            write_info_as.dstSet = *rtx_descriptor_set;

            vk::WriteDescriptorSetAccelerationStructureNV write_as;
            write_as.accelerationStructureCount = 1;
            write_as.pAccelerationStructures = &*engine->game->scene->top_level_as[image]->acceleration_structure;
            write_info_as.pNext = &write_as;

            vk::DescriptorBufferInfo input_buffer_info;
            input_buffer_info.buffer = input_buffer->buffer;
            input_buffer_info.offset = 0;
            input_buffer_info.range = sizeof(RaytraceInput) * queries.size();

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
            output_buffer_info.range = sizeof(RaytraceOutput) * queries.size();

            vk::WriteDescriptorSet write_info_output;
            write_info_output.descriptorCount = 1;
            write_info_output.descriptorType = vk::DescriptorType::eStorageBuffer;
            write_info_output.dstBinding = 2;
            write_info_output.dstArrayElement = 0;
            write_info_output.pBufferInfo = &output_buffer_info;
            write_info_output.dstSet = *rtx_descriptor_set;

            std::vector<vk::WriteDescriptorSet> writes = { write_info_as, write_info_input, write_info_output };
            engine->renderer.device->updateDescriptorSets(writes, nullptr, engine->renderer.dispatch);

            buffer[0]->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingNV, *rtx_pipeline_layout, 0, *rtx_descriptor_set, {}, engine->renderer.dispatch);
            auto shader_size = engine->renderer.ray_tracing_properties.shaderGroupHandleSize;
            buffer[0]->traceRaysNV(shader_binding_table->buffer, 0,
                shader_binding_table->buffer, shader_size, shader_size,
                shader_binding_table->buffer, shader_size, shader_size,
                nullptr, 0, 0, queries.size(), 1, 1, engine->renderer.dispatch);
            buffer[0]->end(engine->renderer.dispatch);
            vk::SubmitInfo submit_info = {};
            submit_info.pCommandBuffers = &*buffer[0];
            submit_info.commandBufferCount = 1;
            raytrace_query_queue.submit(submit_info, *fence, engine->renderer.dispatch);
            engine->renderer.device->waitForFences(*fence, true, std::numeric_limits<uint64_t>::max(), engine->renderer.dispatch);
            engine->renderer.device->resetFences(*fence);

            RaytraceOutput* output_mapped = static_cast<RaytraceOutput*>(output_buffer->map(0, output_buffer_size, {}));
            for (size_t i = 0; i < queries.size(); ++i)
            {
                const auto& query = queries[i];
                query.callback(output_mapped[i].intersection_dist);
            }
            output_buffer->unmap();
        }
        queries.clear();
    }
}
