#pragma once

#include "work_item.h"
#include <memory>
#include <functional>

namespace lotus
{
    class Engine;
    class BackgroundWork : public WorkItem
    {
    public:
        BackgroundWork(std::unique_ptr<WorkItem>&& work, std::function<void(Engine*)>);
        bool Processed() { return processed; };
        bool Finished() { return finished; };
        virtual void Process(WorkerThread*) override;
        void Callback(Engine*);
    private:
        std::unique_ptr<WorkItem> work;
        std::function<void(Engine*)> callback;
        bool processed {false};
        bool finished {false};
    };
}
