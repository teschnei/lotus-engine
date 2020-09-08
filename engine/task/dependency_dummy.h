#pragma once
#include "../work_item.h"

namespace lotus
{
    class DependencyDummyTask : public WorkItem
    {
    public:
        DependencyDummyTask(std::vector<UniqueWork>&& dependencies);
        virtual void Process(WorkerThread*) override;
    private:
        std::vector<UniqueWork> children;
    };
}
