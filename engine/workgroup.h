#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <ranges>
#include "background_work.h"

namespace lotus
{
    class WorkerPool;
    class WorkGroup
    {
    public:
        template<typename Container, typename Callback>
        WorkGroup(Container& _work, Callback cb) : callback(cb), work_size(_work.size())
        {
            std::ranges::transform(_work, std::back_inserter(work), [this](auto& work_item)
            {
                return std::make_unique<BackgroundWork>(std::move(work_item), [this](Engine* engine)
                {
                    std::scoped_lock lk(group_mutex);
                    finished_count++;
                    if (finished_count >= work_size)
                    {
                        callback(engine);
                    }
               });
            });
        }

        WorkGroup(const WorkGroup&) = delete;
        WorkGroup(WorkGroup&&) = delete;
        WorkGroup& operator=(const WorkGroup&) = delete;
        WorkGroup& operator=(WorkGroup&&) = delete;

        bool Finished();

        void QueueItems(WorkerPool*);

    private:
        std::vector<std::unique_ptr<BackgroundWork>> work;
        std::mutex group_mutex;
        std::function<void(Engine*)> callback;
        size_t work_size;
        size_t finished_count{0};
    };
}
