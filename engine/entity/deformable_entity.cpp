#include "deformable_entity.h"
#include "engine/core.h"
#include "engine/entity/component/animation_component.h"
#include "engine/renderer/raytrace_query.h"

namespace lotus
{
    DeformableEntity::DeformableEntity(Engine* engine) : RenderableEntity(engine)
    {
    }

    Task<> DeformableEntity::addSkeleton(std::unique_ptr<Skeleton>&& skeleton)
    {
        animation_component = co_await addComponent<AnimationComponent>(std::move(skeleton));
    }

    void DeformableEntity::populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i];
            BottomLevelAccelerationStructure* blas = nullptr;
            uint32_t resource_index = 0;
            if (model->weighted)
            {
                blas = animation_component->transformed_geometries[i].bottom_level_as[image_index].get();
                resource_index = animation_component->transformed_geometries[i].resource_index;
            }
            else if (model->bottom_level_as)
            {
                blas = model->bottom_level_as.get();
                resource_index = model->resource_index;
            }
            if (blas)
            {
                auto matrix = glm::mat3x4{ glm::transpose(getModelMatrix()) };
                engine->renderer->populateAccelerationStructure(as, blas, matrix, resource_index, static_cast<uint32_t>(Raytracer::ObjectFlags::DynamicEntities), 0);
            }
        }
    }

    void DeformableEntity::update_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i];
            BottomLevelAccelerationStructure* blas = nullptr;
            if (model->weighted)
            {
                blas = animation_component->transformed_geometries[i].bottom_level_as[image_index].get();
            }
            else if (model->bottom_level_as)
            {
                blas = model->bottom_level_as.get();
            }
            if (blas)
            {
                as->UpdateInstance(blas->instanceid, glm::mat3x4{ getModelMatrix() });
            }
        }
    }

    Task<> DeformableEntity::render(Engine* engine, std::shared_ptr<Entity> sp)
    {
        co_await renderWork();
    }

    WorkerTask<> DeformableEntity::renderWork()
    {
        auto image_index = engine->renderer->getCurrentImage();
        updateUniformBuffer(image_index);
        updateAnimationVertices(image_index);

        if (engine->config->renderer.RasterizationEnabled())
        {
            engine->worker_pool->command_buffers.graphics_secondary.queue(*command_buffers[image_index]);
            if (!shadowmap_buffers.empty())
                engine->worker_pool->command_buffers.shadowmap.queue(*shadowmap_buffers[image_index]);
        }
        co_return;
    }

    void DeformableEntity::updateAnimationVertices(int image_index)
    {
        auto component = animation_component;
        auto& skeleton = component->skeleton;

        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *engine->renderer->compute_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);
        auto command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info);

        command_buffer->bindPipeline(vk::PipelineBindPoint::eCompute, *engine->renderer->animation_pipeline);

        vk::DescriptorBufferInfo skeleton_buffer_info;
        skeleton_buffer_info.buffer = animation_component->skeleton_bone_buffer->buffer;
        skeleton_buffer_info.offset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * image_index;
        skeleton_buffer_info.range = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size();

        vk::WriteDescriptorSet skeleton_descriptor_set = {};

        skeleton_descriptor_set.dstSet = nullptr;
        skeleton_descriptor_set.dstBinding = 1;
        skeleton_descriptor_set.dstArrayElement = 0;
        skeleton_descriptor_set.descriptorType = vk::DescriptorType::eStorageBuffer;
        skeleton_descriptor_set.descriptorCount = 1;
        skeleton_descriptor_set.pBufferInfo = &skeleton_buffer_info;

        command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *engine->renderer->animation_pipeline_layout, 0, skeleton_descriptor_set);

        //transform skeleton with current animation
        for (size_t i = 0; i < models.size(); ++i)
        {
            for (size_t j = 0; j < models[i]->meshes.size(); ++j)
            {
                auto& mesh = models[i]->meshes[j];
                auto& vertex_buffer = component->transformed_geometries[i].vertex_buffers[j][image_index];

                vk::DescriptorBufferInfo vertex_weights_buffer_info;
                vertex_weights_buffer_info.buffer = mesh->vertex_buffer->buffer;
                vertex_weights_buffer_info.offset = 0;
                vertex_weights_buffer_info.range = VK_WHOLE_SIZE;

                vk::DescriptorBufferInfo vertex_output_buffer_info;
                vertex_output_buffer_info.buffer = vertex_buffer->buffer;
                vertex_output_buffer_info.offset = 0;
                vertex_output_buffer_info.range = VK_WHOLE_SIZE;

                vk::WriteDescriptorSet weight_descriptor_set {};
                weight_descriptor_set.dstSet = nullptr;
                weight_descriptor_set.dstBinding = 0;
                weight_descriptor_set.dstArrayElement = 0;
                weight_descriptor_set.descriptorType = vk::DescriptorType::eStorageBuffer;
                weight_descriptor_set.descriptorCount = 1;
                weight_descriptor_set.pBufferInfo = &vertex_weights_buffer_info;

                vk::WriteDescriptorSet output_descriptor_set {};
                output_descriptor_set.dstSet = nullptr;
                output_descriptor_set.dstBinding = 2;
                output_descriptor_set.dstArrayElement = 0;
                output_descriptor_set.descriptorType = vk::DescriptorType::eStorageBuffer;
                output_descriptor_set.descriptorCount = 1;
                output_descriptor_set.pBufferInfo = &vertex_output_buffer_info;

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
            if (engine->config->renderer.RaytraceEnabled())
            {
                if (component->transformed_geometries[i].bottom_level_as[image_index])
                    component->transformed_geometries[i].bottom_level_as[image_index]->Update(*command_buffer);
            }
        }
        command_buffer->end();

        engine->worker_pool->command_buffers.compute.queue(*command_buffer);
        engine->worker_pool->gpuResource(std::move(command_buffer));
    }
}
