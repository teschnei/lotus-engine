#include "dependency_dummy.h"
#include "../worker_thread.h"
#include "../core.h"

lotus::DependencyDummyTask::DependencyDummyTask(std::vector<UniqueWork>&& dependencies) :
    WorkItem(), children(std::move(dependencies))
{
}

void lotus::DependencyDummyTask::Process(WorkerThread* thread)
{
    thread->engine->worker_pool->addForegroundWork(children);
}
