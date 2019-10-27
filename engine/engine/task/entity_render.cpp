#include "entity_render.h"
#include "../worker_thread.h"
#include "engine/core.h"
#include "engine/entity/renderable_entity.h"

#include "engine/game.h"
#include "../../../ffxi/dat/os2.h"
#include "engine/entity/component/animation_component.h"

namespace lotus
{
    glm::vec3 EntityRenderTask::mirrorVec(glm::vec3 pos, uint8_t mirror_axis)
    {
        glm::vec3 ret = pos;
        if (mirror_axis == 1)
        {
            ret.x = -ret.x;
        }
        if (mirror_axis == 2)
        {
            ret.y = -ret.y;
        }
        if (mirror_axis == 3)
        {
            ret.z = -ret.z;
        }
        return ret;
    }

    EntityRenderTask::EntityRenderTask(std::shared_ptr<RenderableEntity>& _entity) : WorkItem(), entity(_entity)
    {
        priority = 1;
    }

    void EntityRenderTask::Process(WorkerThread* thread)
    {
        auto image_index = thread->engine->renderer.getCurrentImage();
        updateUniformBuffer(thread, image_index, entity.get());
        if (entity->animation_component)
        {
            updateAnimationVertices(thread, image_index, entity.get());
        }
        if (thread->engine->renderer.RasterizationEnabled())
        {
            thread->graphics.secondary_buffers[image_index].push_back(*entity->command_buffers[image_index]);
            thread->graphics.shadow_buffers[image_index].push_back(*entity->shadowmap_buffers[image_index]);
        }
    }

    void EntityRenderTask::updateUniformBuffer(WorkerThread* thread, int image_index, RenderableEntity* entity)
    {
        RenderableEntity::UniformBufferObject ubo = {};
        ubo.model = entity->getModelMatrix();

        auto& uniform_buffer = entity->uniform_buffer;
        auto data = thread->engine->renderer.device->mapMemory(uniform_buffer->memory, uniform_buffer->memory_offset, sizeof(ubo), {}, thread->engine->renderer.dispatch);
            memcpy(static_cast<uint8_t*>(data)+(image_index*sizeof(ubo)), &ubo, sizeof(ubo));
        thread->engine->renderer.device->unmapMemory(uniform_buffer->memory, thread->engine->renderer.dispatch);
    }

    void EntityRenderTask::updateAnimationVertices(WorkerThread* thread, int image_index, RenderableEntity* entity)
    {
        auto component = entity->animation_component;
        auto& skeleton = component->skeleton;

        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = *thread->compute.command_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);
        command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info, thread->engine->renderer.dispatch);

        command_buffer->bindPipeline(vk::PipelineBindPoint::eCompute, *thread->engine->renderer.animation_pipeline, thread->engine->renderer.dispatch);

        vk::DescriptorBufferInfo skeleton_buffer_info;
        skeleton_buffer_info.buffer = entity->animation_component->skeleton_bone_buffer->buffer;
        skeleton_buffer_info.offset = 0;
        skeleton_buffer_info.range = VK_WHOLE_SIZE;

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
                auto& vertex_buffer = component->acceleration_structures[i].vertex_buffers[j][image_index];

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
                component->acceleration_structures[i].bottom_level_as[image_index]->Update(*command_buffer);
            }
        }
        command_buffer->end(thread->engine->renderer.dispatch);

        thread->compute.primary_buffers[thread->engine->renderer.getCurrentImage()].push_back(*command_buffer);
    }
}