#include "deformed_mesh_component.h"
#include "engine/core.h"

namespace lotus::Component
{
    DeformedMeshComponent::DeformedMeshComponent(Entity* _entity, Engine* _engine, AnimationComponent& animation_component, std::vector<std::shared_ptr<Model>> _models) :
        Component(_entity, _engine, animation_component), models(_models)
    {
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

        for (const auto& model : models)
        {
            if (model->weighted)
            {
                model_transforms.push_back(initModelWork(*command_buffer, *model));
            }
        }
        command_buffer->end();
        engine->worker_pool->command_buffers.compute.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(command_buffer));

        co_return;
    }

    DeformedMeshComponent::ModelTransformedGeometry DeformedMeshComponent::initModelWork(vk::CommandBuffer command_buffer, const Model& model) const
    {
        ModelTransformedGeometry model_transform;
        model_transform.vertex_buffers.resize(model.meshes.size());
        for (size_t i = 0; i < model.meshes.size(); ++i)
        {
            const auto& mesh = model.meshes[i];

            for (uint32_t image = 0; image < engine->renderer->getFrameCount(); ++image)
            {
                size_t vertex_size = mesh->getVertexInputBindingDescription()[0].stride;
                model_transform.vertex_buffers[i].push_back(engine->renderer->gpu->memory_manager->GetBuffer(mesh->getVertexCount() * vertex_size,
                    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, vk::MemoryPropertyFlagBits::eDeviceLocal));
            }
        }

        //TODO: transform with a default t-pose instead of current animation to improve acceleration structure build
        //make sure all vertex and index buffers are finished transferring
        vk::MemoryBarrier2KHR barrier
        {
            .srcStageMask = vk::PipelineStageFlagBits2KHR::eTransfer,
            .srcAccessMask = vk::AccessFlagBits2KHR::eTransferWrite,
            .dstStageMask =  vk::PipelineStageFlagBits2KHR::eComputeShader,
            .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead
        };

        command_buffer.pipelineBarrier2KHR({
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &barrier
        });

        auto& [animation_component] = dependencies;
        auto& skeleton = animation_component.skeleton;
        command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *engine->renderer->animation_pipeline);

        for (uint32_t image_index = 0; image_index < engine->renderer->getFrameCount(); ++image_index)
        {
            vk::DescriptorBufferInfo skeleton_buffer_info {
                .buffer = animation_component.skeleton_bone_buffer->buffer,
                .offset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * image_index,
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

            for (size_t j = 0; j < model.meshes.size(); ++j)
            {
                auto& mesh = model.meshes[j];
                auto& vertex_buffer = model_transform.vertex_buffers[j][image_index];

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
                    .srcStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                    .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                    .dstStageMask = vk::PipelineStageFlagBits2KHR::eAccelerationStructureBuild,
                    .dstAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureRead,
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
        return model_transform;
    }

    WorkerTask<> DeformedMeshComponent::tick(time_point time, duration delta)
    {
        auto& [animation_component] = dependencies;
        auto& skeleton = animation_component.skeleton;
        auto image_index = engine->renderer->getCurrentFrame();

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique({
            .commandPool = *engine->renderer->compute_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        });
        auto command_buffer = std::move(command_buffers[0]);

        command_buffer->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        command_buffer->bindPipeline(vk::PipelineBindPoint::eCompute, *engine->renderer->animation_pipeline);

        vk::DescriptorBufferInfo skeleton_buffer_info {
            .buffer = animation_component.skeleton_bone_buffer->buffer,
            .offset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * image_index,
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
            for (size_t j = 0; j < models[i]->meshes.size(); ++j)
            {
                auto& mesh = models[i]->meshes[j];
                auto& vertex_buffer = model_transforms[i].vertex_buffers[j][image_index];

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
                    .srcStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                    .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                    .dstStageMask = vk::PipelineStageFlagBits2KHR::eAccelerationStructureBuild,
                    .dstAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureWrite| vk::AccessFlagBits2KHR::eAccelerationStructureRead,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer = vertex_buffer->buffer,
                    .size = VK_WHOLE_SIZE
                };

                command_buffer->pipelineBarrier2KHR({
                    .bufferMemoryBarrierCount = 1,
                    .pBufferMemoryBarriers = &barrier
                });
            }
        }
        command_buffer->end();

        engine->worker_pool->command_buffers.compute.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(command_buffer));
        co_return;
    }

    const DeformedMeshComponent::ModelTransformedGeometry& DeformedMeshComponent::getModelTransformGeometry(size_t index) const
    {
        return model_transforms[index];
    }

    void DeformedMeshComponent::updateModelTransformGeometryResourceIndex(size_t index, uint32_t resource_index)
    {
        model_transforms[index].resource_index = resource_index;
    }

    std::vector<std::shared_ptr<Model>> DeformedMeshComponent::getModels() const
    {
        return models;
    }

    WorkerTask<DeformedMeshComponent::ModelTransformedGeometry> DeformedMeshComponent::initModel(std::shared_ptr<Model> model) const
    {
        ModelTransformedGeometry transform;
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
            transform = initModelWork(*command_buffer, *model);
        }
        command_buffer->end();
        engine->worker_pool->command_buffers.compute.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(command_buffer));
        co_return std::move(transform);
    }

    void DeformedMeshComponent::replaceModelIndex(std::shared_ptr<Model> model, ModelTransformedGeometry&& transform, uint32_t index)
    {
        std::swap(models[index], model);
        std::swap(model_transforms[index], transform);

        engine->worker_pool->gpuResource(std::move(model));
        engine->worker_pool->gpuResource(std::move(transform));
    }
}
