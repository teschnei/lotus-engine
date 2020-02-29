#pragma once

#include "../work_item.h"
#include "engine/entity/renderable_entity.h"

namespace lotus
{
    class TransformSkeletonTask : public WorkItem
    {
    public:
        TransformSkeletonTask(RenderableEntity* entity);
        virtual ~TransformSkeletonTask() override = default;
        virtual void Process(WorkerThread*) override;

    private:
        RenderableEntity* entity;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
        std::unique_ptr<Buffer> staging_buffer;
    };
}
