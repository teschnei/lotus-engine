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
            thread->secondary_buffers[image_index].push_back(*entity->command_buffers[image_index]);
            thread->shadow_buffers[image_index].push_back(*entity->shadowmap_buffers[image_index]);
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
        alloc_info.commandPool = *thread->command_pool;
        alloc_info.commandBufferCount = 1;

        auto command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);
        command_buffer = std::move(command_buffers[0]);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        command_buffer->begin(begin_info, thread->engine->renderer.dispatch);

        //transform skeleton with current animation   
        for (size_t i = 0; i < entity->models.size(); ++i)
        {
            for (size_t j = 0; j < entity->models[i]->meshes.size(); ++j)
            {
                auto& mesh = entity->models[i]->meshes[j];
                auto weighted_vertex_buffer = static_cast<FFXI::OS2::WeightingVertex*>(thread->engine->renderer.device->mapMemory(mesh->vertex_buffer->memory, mesh->vertex_buffer->memory_offset, VK_WHOLE_SIZE, {}, thread->engine->renderer.dispatch));
                std::vector<FFXI::OS2::Vertex> vertices;

                for (size_t w = 0; w < mesh->getVertexCount(); w += 2)
                {
                    auto vertex1 = weighted_vertex_buffer[w];
                    auto vertex2 = weighted_vertex_buffer[w+1];

                    FFXI::OS2::Vertex os2_vertex {};

                    //TODO: move this to compute shader
                    if (vertex2.weight == 0.f)
                    {
                        Skeleton::Bone& bone = skeleton->bones[vertex1.bone_index];
                        os2_vertex.pos = bone.rot * mirrorVec(vertex1.pos, vertex1.mirror_axis) + bone.trans;
                        os2_vertex.pos *= bone.scale;
                        os2_vertex.norm = (bone.rot * vertex1.norm);
                        os2_vertex.norm = glm::normalize(os2_vertex.norm);
                    }
                    else
                    {
                        Skeleton::Bone& bone1 = skeleton->bones[vertex1.bone_index];
                        Skeleton::Bone& bone2 = skeleton->bones[vertex2.bone_index];

                        glm::vec3 pos1 = bone1.rot * mirrorVec(vertex1.pos, vertex1.mirror_axis) + (bone1.trans * vertex1.weight);
                        pos1 *= bone1.scale;
                        glm::vec3 norm1 = bone1.rot * vertex1.norm * vertex1.weight;
                        norm1 = glm::normalize(norm1);

                        glm::vec3 pos2 = (bone2.rot * mirrorVec(vertex2.pos, vertex2.mirror_axis)) + (bone2.trans * vertex2.weight);
                        pos2 *= bone2.scale;
                        glm::vec3 norm2 = (bone2.rot * vertex2.norm) * vertex2.weight;
                        norm2 = glm::normalize(norm2);

                        os2_vertex.pos = pos1 + pos2;
                        os2_vertex.norm = glm::normalize(norm1 + norm2);
                    }
                    os2_vertex.uv = vertex1.uv;
                    vertices.push_back(os2_vertex);
                }
                thread->engine->renderer.device->unmapMemory(mesh->vertex_buffer->memory);

                auto& vertex_buffer = component->acceleration_structures[i].vertex_buffers[j][image_index];
                staging_buffers.push_back(thread->engine->renderer.memory_manager->GetBuffer(vertices.size() * sizeof(FFXI::OS2::Vertex), vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
                uint8_t* staging_buffer_data = static_cast<uint8_t*>(thread->engine->renderer.device->mapMemory(staging_buffers.back()->memory, staging_buffers.back()->memory_offset, vertices.size() * sizeof(FFXI::OS2::Vertex), {}, thread->engine->renderer.dispatch));
                memcpy(staging_buffer_data, vertices.data(), vertices.size() * sizeof(FFXI::OS2::Vertex));
                thread->engine->renderer.device->unmapMemory(staging_buffers.back()->memory);

                vk::BufferMemoryBarrier barrier;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = vertex_buffer->buffer;
                barrier.size = VK_WHOLE_SIZE;
                barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteNV | vk::AccessFlagBits::eAccelerationStructureReadNV;

                command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, {}, nullptr, barrier, nullptr, thread->engine->renderer.dispatch);

                vk::BufferCopy copy_region;
                copy_region.srcOffset = 0;
                copy_region.size = vertices.size() * sizeof(FFXI::OS2::Vertex);
                command_buffer->copyBuffer(staging_buffers.back()->buffer, vertex_buffer->buffer, copy_region, thread->engine->renderer.dispatch);
            }
            if (thread->engine->renderer.RTXEnabled())
            {
                component->acceleration_structures[i].bottom_level_as[image_index]->Update(*command_buffer);
            }
        }
        command_buffer->end(thread->engine->renderer.dispatch);

        thread->primary_buffers[thread->engine->renderer.getCurrentImage()].push_back(*command_buffer);
    }
}
