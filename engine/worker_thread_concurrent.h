#pragma once

#include "worker_thread.h"
#include <thread>

namespace lotus
{
    class WorkerThreadConcurrent : public WorkerThread
    {
    public:
        WorkerThreadConcurrent(Engine*, WorkerPool*);
        ~WorkerThreadConcurrent() = default;

        void Join();

    protected:
        void WorkLoop();
        std::thread thread{ &WorkerThreadConcurrent::WorkLoop, this };
    };
}
