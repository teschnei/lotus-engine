#include "entity_render.h"
#include "../worker_thread.h"
#include "core.h"
#include "entity/renderable_entity.h"

namespace lotus
{
    EntityRenderTask::EntityRenderTask(std::shared_ptr<RenderableEntity>& _entity) : WorkItem(), entity(_entity)
    {
        priority = 1;
    }

    void EntityRenderTask::Process(WorkerThread* thread)
    {
        auto image_index = thread->engine->renderer.getCurrentImage();
        updateUniformBuffer(thread, image_index, entity.get());
        thread->secondary_buffers[image_index].push_back(*entity->command_buffers[image_index]);
        //for (const auto& buffer : entity->blended_buffers[image_index])
        //{
        //    thread->blended_buffers[image_index].emplace_back(buffer.first, *buffer.second);
        //}
    }

    void EntityRenderTask::updateUniformBuffer(WorkerThread* thread, int image_index, RenderableEntity* entity)
    {
        RenderableEntity::UniformBufferObject ubo = {};
        ubo.model = entity->getModelMatrix();
        ubo.view = thread->engine->camera.getViewMatrix();
        ubo.proj = thread->engine->camera.getProjMatrix();

        auto& uniform_buffer = entity->uniform_buffers[image_index];
        auto data = thread->engine->renderer.device->mapMemory(uniform_buffer->memory, uniform_buffer->memory_offset, sizeof(ubo), {}, thread->engine->renderer.dispatch);
            memcpy(data, &ubo, sizeof(ubo));
        thread->engine->renderer.device->unmapMemory(uniform_buffer->memory, thread->engine->renderer.dispatch);

    }
}
