#include "renderable_entity_init.h"
#include "engine/worker_thread.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/entity_initializers/renderable_entity.h"

namespace lotus
{
    RenderableEntityInitTask::RenderableEntityInitTask(const std::shared_ptr<RenderableEntity>& _entity) : WorkItem(), entity(_entity)
    {
        initializer = std::make_unique<RenderableEntityInitializer>(entity.get(), this);
    }

    void RenderableEntityInitTask::Process(WorkerThread* thread)
    {
        thread->engine->renderer->initEntity(initializer.get(), thread);
        thread->engine->renderer->drawEntity(initializer.get(), thread);
    }

    RenderableEntityReInitTask::RenderableEntityReInitTask(const std::shared_ptr<RenderableEntity>& entity) : RenderableEntityInitTask(entity)
    {
    }

    void RenderableEntityReInitTask::Process(WorkerThread* thread)
    {
        thread->engine->renderer->drawEntity(initializer.get(), thread);
    }
}
