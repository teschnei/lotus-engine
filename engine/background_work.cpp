#include "background_work.h"

#include "engine/core.h"
#include "engine/worker_pool.h"

namespace lotus
{
    BackgroundWork::BackgroundWork(std::unique_ptr<WorkItem>&& _work, std::function<void(Engine*)> _callback) : WorkItem(), work(std::move(_work)),  callback(_callback)
    {
    }

    void BackgroundWork::Process(WorkerThread* thread)
    {
        work->Process(thread);
        if (!work->children_work.empty())
        {
            children_finished = false;
            thread->pool->addBackgroundWork(work->children_work, [this](Engine*){children_finished = true;});
        }
        processed = true;
    }

    void BackgroundWork::Callback(Engine* engine)
    {
        callback(engine);
        callback_finished = true;
    }
}
