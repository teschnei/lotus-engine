#pragma once

#include "../work_item.h"
#include "engine/renderer/acceleration_structure.h"

namespace lotus
{
    class AccelerationBuildTask : public WorkItem
    {
    public:
        AccelerationBuildTask(int image_index, const std::shared_ptr<TopLevelAccelerationStructure>& as);
        virtual ~AccelerationBuildTask() override = default;
        virtual void Process(WorkerThread*) override;

    private:
        int image_index;
        std::shared_ptr<TopLevelAccelerationStructure> as;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
    };
}
