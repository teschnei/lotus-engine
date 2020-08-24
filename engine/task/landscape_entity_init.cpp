#include "landscape_entity_init.h"

#include "engine/worker_thread.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/camera.h"
#include "engine/renderer/vulkan/entity_initializers/landscape_entity.h"

namespace lotus
{
    LandscapeEntityInitTask::LandscapeEntityInitTask(const std::shared_ptr<LandscapeEntity>& _entity, std::vector<LandscapeEntity::InstanceInfo>&& _instance_info) :
        WorkItem(), entity(_entity)
    {
        initializer = std::make_unique<LandscapeEntityInitializer>(_entity.get(), std::move(_instance_info), this);
    }

    void LandscapeEntityInitTask::Process(WorkerThread* thread)
    {
        thread->engine->renderer->initEntity(initializer.get(), thread);
        thread->engine->renderer->drawEntity(initializer.get(), thread);
    }

    LandscapeEntityReInitTask::LandscapeEntityReInitTask(const std::shared_ptr<LandscapeEntity>& entity) : LandscapeEntityInitTask(entity, {})
    {
    }

    void LandscapeEntityReInitTask::Process(WorkerThread* thread)
    {
        thread->engine->renderer->drawEntity(initializer.get(), thread);
    }
}
