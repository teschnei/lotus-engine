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
        BackgroundWork(UniqueWork&& work, std::function<void(Engine*)>);
        bool Processed() { return processed && children_finished; };
        bool Finished() { return callback_finished && children_finished; };
        virtual void Process(WorkerThread*) override;
        void Callback(Engine*);
        UniqueWork&& GetWork() { return std::move(work); }
    private:
        UniqueWork work;
        std::function<void(Engine*)> callback;
        bool processed {false};
        bool callback_finished {false};
        bool children_finished {true};
    };
    using UniqueBackgroundWork = std::unique_ptr<BackgroundWork>;
}
