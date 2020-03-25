#pragma once

#include "../work_item.h"
#include "engine/entity/deformable_entity.h"

namespace lotus
{
    class TransformSkeletonTask : public WorkItem
    {
    public:
        TransformSkeletonTask(DeformableEntity* entity);
        virtual ~TransformSkeletonTask() override = default;
        virtual void Process(WorkerThread*) override;

    private:
        DeformableEntity* entity;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
        std::unique_ptr<Buffer> staging_buffer;
    };
}
