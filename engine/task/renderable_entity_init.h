#pragma once
#include "engine/work_item.h"
#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/renderable_entity.h"

namespace lotus
{
    class DeformableEntity;
    class RenderableEntityInitTask : public WorkItem
    {
    public:
        RenderableEntityInitTask(const std::shared_ptr<RenderableEntity>& entity);
        virtual void Process(WorkerThread*) override;
    protected:
        std::unique_ptr<EntityInitializer> initializer;
        std::shared_ptr<RenderableEntity> entity;
    };


    class RenderableEntityReInitTask : public RenderableEntityInitTask
    {
    public:
        RenderableEntityReInitTask(const std::shared_ptr<RenderableEntity>& entity);
        virtual void Process(WorkerThread*) override;
    };

}
