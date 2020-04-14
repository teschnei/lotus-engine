#pragma once

#include <memory>
#include "engine/work_item.h"
#include "engine/entity/deformable_entity.h"

namespace lotus
{
    class TransformSkeletonTask : public WorkItem
    {
    public:
        TransformSkeletonTask(std::shared_ptr<DeformableEntity> entity);
        virtual ~TransformSkeletonTask() override = default;
        virtual void Process(WorkerThread*) override;

    private:
        std::shared_ptr<DeformableEntity> entity;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
        std::unique_ptr<Buffer> staging_buffer;
    };
}
