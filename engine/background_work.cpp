#include "background_work.h"

namespace lotus
{
    BackgroundWork::BackgroundWork(std::unique_ptr<WorkItem>&& _work, std::function<void(Engine*)> _callback) : WorkItem(), work(std::move(_work)),  callback(_callback)
    {

    }

    void BackgroundWork::Process(WorkerThread* thread)
    {
        work->Process(thread);
        processed = true;
    }

    void BackgroundWork::Callback(Engine* engine)
    {
        callback(engine);
        finished = true;
    }
}
