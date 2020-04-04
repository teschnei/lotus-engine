#include "entity_render.h"
#include "../worker_thread.h"
#include "engine/core.h"
#include "engine/entity/renderable_entity.h"
#include "engine/entity/deformable_entity.h"
#include "engine/entity/particle.h"

#include "engine/game.h"
#include "engine/entity/component/animation_component.h"

namespace lotus
{
    EntityRenderTask::EntityRenderTask(std::shared_ptr<RenderableEntity>& _entity, float _priority) : WorkItem(), entity(_entity)
    {
        priority = _priority;
    }

    void EntityRenderTask::Process(WorkerThread* thread)
    {
        auto image_index = thread->engine->renderer.getCurrentImage();
        updateUniformBuffer(thread, image_index, entity.get());
        if (auto deformable = dynamic_cast<DeformableEntity*>(entity.get()))
        {
            updateAnimationVertices(thread, image_index, deformable);
        }
        if (thread->engine->renderer.RasterizationEnabled())
        {
            if (dynamic_cast<Particle*>(entity.get()))
            {
                graphics.particle = *entity->command_buffers[image_index];
            }
            else
            {
                graphics.secondary = *entity->command_buffers[image_index];
                graphics.shadow = *entity->shadowmap_buffers[image_index];
            }
        }
    }

    void EntityRenderTask::updateUniformBuffer(WorkerThread* thread, int image_index, RenderableEntity* entity)
    {
        RenderableEntity::UniformBufferObject ubo = {};
        ubo.model = entity->getModelMatrix();
        ubo.modelIT = glm::transpose(glm::inverse(glm::mat3(ubo.model)));

        auto& uniform_buffer = entity->uniform_buffer;
        auto data = uniform_buffer->map(0, sizeof(ubo) * thread->engine->renderer.getImageCount(), {});
            memcpy(static_cast<uint8_t*>(data)+(image_index*sizeof(ubo)), &ubo, sizeof(ubo));
        uniform_buffer->unmap();
    }

    void EntityRenderTask::updateAnimationVertices(WorkerThread* thread, int image_index, DeformableEntity* entity)
    {
        auto component = entity->animation_component;
        auto& skeleton = component->skeleton;

        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *thread->compute_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);
        command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info, thread->engine->renderer.dispatch);

        command_buffer->bindPipeline(vk::PipelineBindPoint::eCompute, *thread->engine->renderer.animation_pipeline, thread->engine->renderer.dispatch);

        vk::DescriptorBufferInfo skeleton_buffer_info;
        skeleton_buffer_info.buffer = entity->animation_component->skeleton_bone_buffer->buffer;
        skeleton_buffer_info.offset = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size() * image_index;
        skeleton_buffer_info.range = sizeof(AnimationComponent::BufferBone) * skeleton->bones.size();

        vk::WriteDescriptorSet skeleton_descriptor_set = {};

        skeleton_descriptor_set.dstSet = nullptr;
        skeleton_descriptor_set.dstBinding = 1;
        skeleton_descriptor_set.dstArrayElement = 0;
        skeleton_descriptor_set.descriptorType = vk::DescriptorType::eStorageBuffer;
        skeleton_descriptor_set.descriptorCount = 1;
        skeleton_descriptor_set.pBufferInfo = &skeleton_buffer_info;

        command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *thread->engine->renderer.animation_pipeline_layout, 0, skeleton_descriptor_set, thread->engine->renderer.dispatch);

        //transform skeleton with current animation   
        for (size_t i = 0; i < entity->models.size(); ++i)
        {
            for (size_t j = 0; j < entity->models[i]->meshes.size(); ++j)
            {
                auto& mesh = entity->models[i]->meshes[j];
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

                command_buffer->pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *thread->engine->renderer.animation_pipeline_layout, 0, {weight_descriptor_set, output_descriptor_set}, thread->engine->renderer.dispatch);

                command_buffer->dispatch(mesh->getVertexCount(), 1, 1, thread->engine->renderer.dispatch);

                vk::BufferMemoryBarrier barrier;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = vertex_buffer->buffer;
                barrier.size = VK_WHOLE_SIZE;
                barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteNV | vk::AccessFlagBits::eAccelerationStructureReadNV;

                command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, {}, nullptr, barrier, nullptr, thread->engine->renderer.dispatch);
            }
            if (thread->engine->renderer.RTXEnabled())
            {
                component->transformed_geometries[i].bottom_level_as[image_index]->Update(*command_buffer);
            }
        }
        command_buffer->end(thread->engine->renderer.dispatch);

        compute.primary = *command_buffer;
    }
}
