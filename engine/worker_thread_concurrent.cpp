#include "worker_thread_concurrent.h"

#include "worker_pool.h"

namespace lotus
{
    WorkerThreadConcurrent::WorkerThreadConcurrent(Engine* engine, WorkerPool* pool) : WorkerThread(engine, pool)
    {

    }

    void WorkerThreadConcurrent::Join()
    {
        thread.join();
    }

    void WorkerThreadConcurrent::WorkLoop()
    {
        while (active)
        {
            work = pool->waitForWork();
            if (work)
            {
                work->Process(this);
                if (!work->children_work.empty())
                {
                    pool->addForegroundWork(work->children_work);
                }
                pool->workFinished(std::move(work));
            }
        }
    }
}
