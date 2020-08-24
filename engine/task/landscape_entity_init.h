#pragma once
#include "engine/work_item.h"
#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/landscape_entity.h"

namespace lotus
{
    class LandscapeEntityInitializer;
    class LandscapeEntityInitTask : public WorkItem
    {
    public:
        LandscapeEntityInitTask(const std::shared_ptr<LandscapeEntity>& entity, std::vector<LandscapeEntity::InstanceInfo>&& instance_info);
        virtual void Process(WorkerThread*) override;
    protected:
        std::unique_ptr<EntityInitializer> initializer;
        std::shared_ptr<LandscapeEntity> entity;
    };

    class LandscapeEntityReInitTask : public LandscapeEntityInitTask
    {
    public:
        LandscapeEntityReInitTask(const std::shared_ptr<LandscapeEntity>& entity);
        virtual void Process(WorkerThread*) override;
    };
}
