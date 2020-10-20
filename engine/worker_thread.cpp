#include "worker_thread.h"
#include "worker_pool.h"
#include "core.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus
{
    WorkerThread::WorkerThread(Engine* _engine, WorkerPool* _pool) : pool(_pool), engine(_engine), thread(&WorkerThread::WorkLoop, this)
    {
    }

    void WorkerThread::Join()
    {
        thread.join();
    }

}
