#pragma once
#include <functional>

namespace lotus
{
    class WorkerThread;
    class WorkItem
    {
    public:
        WorkItem(){}
        WorkItem(const WorkItem&) = delete;
        WorkItem(WorkItem&&) = default;
        WorkItem& operator=(const WorkItem&) = delete;
        WorkItem& operator=(WorkItem&&) = default;
        virtual void Process(WorkerThread*) = 0;
        virtual ~WorkItem() = default;

        int priority = 0;
        bool operator>(const WorkItem& o) const { return priority > o.priority; }
    };

    class LambdaWorkItem : public WorkItem
    {
    public:
        LambdaWorkItem(int image, std::function<void()> _function) : WorkItem(), function(std::move(_function)){}
        virtual void Process(WorkerThread*) override { function(); }
    private:
        std::function<void()> function;
    };

}
