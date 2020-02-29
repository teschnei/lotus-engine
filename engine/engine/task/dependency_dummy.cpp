#include "dependency_dummy.h"
#include "../worker_thread.h"
#include "../core.h"

lotus::DependencyDummyTask::DependencyDummyTask(std::vector<std::unique_ptr<WorkItem>>&& dependencies) :
    WorkItem(), children(std::move(dependencies))
{
}

void lotus::DependencyDummyTask::Process(WorkerThread* thread)
{
    thread->engine->worker_pool.addWork(children);
}
