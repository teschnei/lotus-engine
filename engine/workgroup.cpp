#include "workgroup.h"
#include "worker_pool.h"

namespace lotus
{
    bool WorkGroup::Finished()
    {
        if (finished_count >= work_size)
        {
            //if the mutex is taken, wait until it's free (the group may not be ready to destruct yet)
            std::scoped_lock lk(group_mutex);
            return true;
        }
        return false;
    }

    void WorkGroup::QueueItems(WorkerPool* pool)
    {
        pool->addBackgroundWork(work);
    }
}
