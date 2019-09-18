#include "entity_render.h"
#include "../worker_thread.h"
#include "core.h"
#include "entity/renderable_entity.h"

#include "game.h"

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
}
