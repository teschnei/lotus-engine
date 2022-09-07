#include "deformed_mesh_component.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus::Component
{
    DeformedMeshComponent::DeformedMeshComponent(Entity* _entity, Engine* _engine, const RenderBaseComponent& _base_component, 
        const AnimationComponent& _animation_component, std::vector<std::shared_ptr<Model>> _models) :
        Component(_entity, _engine), base_component(_base_component), animation_component(_animation_component)
    {
        for (const auto& model : _models)
        {
            models.push_back({
                .model = model
            });
        }
    }

    WorkerTask<> DeformedMeshComponent::init()
    {
        vk::CommandBufferAllocateInfo alloc_info;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
            .commandPool = *engine->renderer->compute_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        });
        auto command_buffer = std::move(command_buffers[0]);

        command_buffer->begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });

        for (auto& model : models)
        {
            if (model.model->weighted)
            {
                model = initModelWork(*command_buffer, model.model);
            }
        }
        command_buffer->end();
        co_await engine->renderer->async_compute->compute(std::move(command_buffer));

        co_return;
    }

    DeformedMeshComponent::ModelInfo DeformedMeshComponent::initModelWork(vk::CommandBuffer command_buffer, std::shared_ptr<Model> model) const
    {
        ModelInfo info
        {
            .model = model
        };
        info.vertex_buffers.resize(model->meshes.size());
        info.vertex_buffer_indices.resize(model->meshes.size());
        for (uint32_t image = 0; image < engine->renderer->getFrameCount(); ++image)
        {
            info.mesh_infos.push_back(engine->renderer->global_descriptors->getMeshInfoBuffer(model->meshes.size()));
        }
        for (size_t i = 0; i < model->meshes.size(); ++i)
        {
            const auto& mesh = model->meshes[i];

            for (uint32_t image = 0; image < engine->renderer->getFrameCount(); ++image)
            {
                size_t vertex_size = mesh->getVertexInputBindingDescription()[0].stride;
                info.vertex_buffers[i].push_back(engine->renderer->gpu->memory_manager->GetBuffer(mesh->getVertexCount() * vertex_size,
                    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, vk::MemoryPropertyFlagBits::eDeviceLocal));
                auto vertex_index = engine->renderer->global_descriptors->getVertexIndex();
                vertex_index->write({
                    .buffer = info.vertex_buffers[i][image]->buffer,
                    .offset = 0,
                    .range = VK_WHOLE_SIZE
                });
                info.mesh_infos[image]->buffer_view[i] = {
                    .vertex_offset = vertex_index->index,
                    .index_offset = mesh->index_descriptor_index->index,
                    .indices = static_cast<uint32_t>(mesh->getIndexCount()),
                    .material_index = mesh->material->getIndex(),
                    .scale = glm::vec3{1.0},
                    .billboard = 0,
                    .colour = glm::vec4{1.0},
                    .uv_offset = glm::vec2{0.0},
                    .animation_frame = 0,
                    .vertex_prev_offset = 0,
                    .model_prev = glm::mat4{1.0}
                };
                info.vertex_buffer_indices[i].push_back(std::move(vertex_index));
            }
        }

        //TODO: transform with a default t-pose instead of current animation to improve acceleration structure build
        //make sure all vertex and index buffers are finished transferring
        vk::MemoryBarrier2KHR barrier
        {
            .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
            .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
            .dstStageMask =  vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderRead
        };

        command_buffer.pipelineBarrier2KHR({
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &barrier
        });

        auto& skeleton = animation_component.skeleton;
        command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *engine->renderer->animation_pipeline);

        for (uint32_t current_frame = 0; current_frame < engine->renderer->getFrameCount(); ++current_frame)
        {
            vk::DescriptorBufferInfo skeleton_buffer_info {
                .buffer = animation_component.skeleton_bone_buffer->buffer,
                .offset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * current_frame,
                .range = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size()
            };

            vk::WriteDescriptorSet skeleton_descriptor_set {
                .dstSet = nullptr,
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &skeleton_buffer_info
            };

            command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *engine->renderer->animation_pipeline_layout, 0, skeleton_descriptor_set);

            for (size_t j = 0; j < model->meshes.size(); ++j)
            {
                auto& mesh = model->meshes[j];
                auto& vertex_buffer = info.vertex_buffers[j][current_frame];

                vk::DescriptorBufferInfo vertex_weights_buffer_info {
                    .buffer = mesh->vertex_buffer->buffer,
                    .offset = 0,
                    .range = VK_WHOLE_SIZE
                };

                vk::DescriptorBufferInfo vertex_output_buffer_info {
                    .buffer = vertex_buffer->buffer,
                    .offset = 0,
                    .range = VK_WHOLE_SIZE
                };

                vk::WriteDescriptorSet weight_descriptor_set {
                    .dstSet = nullptr,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &vertex_weights_buffer_info
                };

                vk::WriteDescriptorSet output_descriptor_set {
                    .dstSet = nullptr,
                    .dstBinding = 2,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &vertex_output_buffer_info
                };

                command_buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *engine->renderer->animation_pipeline_layout, 0, { weight_descriptor_set, output_descriptor_set });

                command_buffer.dispatch(mesh->getVertexCount(), 1, 1);

                vk::BufferMemoryBarrier2KHR barrier
                {
                    .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                    .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                    .dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
                    .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer = vertex_buffer->buffer,
                    .size = VK_WHOLE_SIZE
                };

                command_buffer.pipelineBarrier2KHR({
                    .bufferMemoryBarrierCount = 1,
                    .pBufferMemoryBarriers = &barrier
                });
            }
        }
        return info;
    }

    WorkerTask<> DeformedMeshComponent::tick(time_point time, duration elapsed)
    {
        auto& skeleton = animation_component.skeleton;
        auto current_frame = engine->renderer->getCurrentFrame();
        auto previous_frame = engine->renderer->getPreviousFrame();

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
            .commandPool = *engine->renderer->graphics_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        });
        auto command_buffer = std::move(command_buffers[0]);

        command_buffer->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        command_buffer->bindPipeline(vk::PipelineBindPoint::eCompute, *engine->renderer->animation_pipeline);

        vk::DescriptorBufferInfo skeleton_buffer_info {
            .buffer = animation_component.skeleton_bone_buffer->buffer,
            .offset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * current_frame,
            .range = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size()
        };

        vk::WriteDescriptorSet skeleton_descriptor_set {
            .dstSet = nullptr,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &skeleton_buffer_info
        };

        command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *engine->renderer->animation_pipeline_layout, 0, skeleton_descriptor_set);

        //transform skeleton with current animation
        for (size_t i = 0; i < models.size(); ++i)
        {
            for (size_t j = 0; j < models[i].model->meshes.size(); ++j)
            {
                auto& mesh = models[i].model->meshes[j];
                auto& vertex_buffer = models[i].vertex_buffers[j][current_frame];

                vk::DescriptorBufferInfo vertex_weights_buffer_info {
                    .buffer = mesh->vertex_buffer->buffer,
                    .offset = 0,
                    .range = VK_WHOLE_SIZE
                };

                vk::DescriptorBufferInfo vertex_output_buffer_info {
                    .buffer = vertex_buffer->buffer,
                    .offset = 0,
                    .range = VK_WHOLE_SIZE
                };

                vk::WriteDescriptorSet weight_descriptor_set {
                    .dstSet = nullptr,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &vertex_weights_buffer_info,
                };

                vk::WriteDescriptorSet output_descriptor_set {
                    .dstSet = nullptr,
                    .dstBinding = 2,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &vertex_output_buffer_info
                };

                command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *engine->renderer->animation_pipeline_layout, 0, {weight_descriptor_set, output_descriptor_set});

                command_buffer->dispatch(mesh->getVertexCount(), 1, 1);

                vk::BufferMemoryBarrier2KHR barrier
                {
                    .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                    .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                    .dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
                    .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR| vk::AccessFlagBits2::eAccelerationStructureReadKHR,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer = vertex_buffer->buffer,
                    .size = VK_WHOLE_SIZE
                };

                command_buffer->pipelineBarrier2KHR({
                    .bufferMemoryBarrierCount = 1,
                    .pBufferMemoryBarriers = &barrier
                });

                auto vertex_index = models[i].vertex_buffer_indices[j][current_frame]->index;
                auto prev_vertex_index = models[i].vertex_buffer_indices[j][previous_frame]->index;

                models[i].mesh_infos[current_frame]->buffer_view[j].vertex_offset = vertex_index;
                models[i].mesh_infos[current_frame]->buffer_view[j].vertex_prev_offset = prev_vertex_index;
                models[i].mesh_infos[current_frame]->buffer_view[j].animation_frame = models[0].model->animation_frame;
                models[i].mesh_infos[current_frame]->buffer_view[j].model_prev = base_component.getPrevModelMatrix();
            }
        }
        command_buffer->end();

        engine->worker_pool->command_buffers.graphics_primary.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(command_buffer));
        co_return;
    }

    std::span<const DeformedMeshComponent::ModelInfo> DeformedMeshComponent::getModels() const
    {
        return models;
    }

    WorkerTask<DeformedMeshComponent::ModelInfo> DeformedMeshComponent::initModel(std::shared_ptr<Model> model) const
    {
        ModelInfo info;
        vk::CommandBufferAllocateInfo alloc_info;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
            .commandPool = *engine->renderer->compute_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        });
        auto command_buffer = std::move(command_buffers[0]);

        command_buffer->begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });

        if (model->weighted)
        {
            info = initModelWork(*command_buffer, model);
        }
        command_buffer->end();
        co_await engine->renderer->async_compute->compute(std::move(command_buffer));
        co_return std::move(info);
    }

    void DeformedMeshComponent::replaceModelIndex(ModelInfo&& info, uint32_t index)
    {
        std::swap(models[index], info);
        engine->worker_pool->gpuResource(std::move(info));
    }
}
