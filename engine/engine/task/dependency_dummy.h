#pragma once
#include "../work_item.h"

namespace lotus
{
    class DependencyDummyTask : public WorkItem
    {
    public:
        DependencyDummyTask(std::vector<std::unique_ptr<WorkItem>>&& dependencies);
        virtual void Process(WorkerThread*) override;
    private:
        std::vector<std::unique_ptr<WorkItem>> children;
    };
}
