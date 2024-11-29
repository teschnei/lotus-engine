module;

#include <coroutine>
#include <utility>
#include <vector>

export module lotus:util.async_queue;

import :util.shared_linked_list;

namespace lotus
{
template <typename T = void> class AsyncQueue
{
public:
    auto wait(T&& data) { return AsyncQueueItem(this, std::forward<T&&>(data)); }
    auto getAll() { return waiting_tasks.getAll(); }
    class AsyncQueueItem
    {
    public:
        AsyncQueueItem(AsyncQueue* _queue, T&& _data) : queue(_queue), data(std::forward<T&&>(_data)) {}

        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> awaiter) noexcept
        {
            awaiting = awaiter;
            queue->waiting_tasks.queue(this);
        }
        auto await_resume() noexcept { return std::move(data); }

        std::coroutine_handle<> awaiting;
        T data;

    private:
        AsyncQueue* queue;
    };

private:
    SharedLinkedList<AsyncQueueItem*> waiting_tasks{};
};

template <> class AsyncQueue<void>
{
public:
    auto wait() { return AsyncQueueItem(this); }
    auto getAll() { return waiting_tasks.getAll(); }
    class AsyncQueueItem
    {
    public:
        AsyncQueueItem(AsyncQueue* _queue) : queue(_queue) {}

        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> awaiter) noexcept
        {
            awaiting = awaiter;
            queue->waiting_tasks.queue(this);
        }
        void await_resume() noexcept {}

        std::coroutine_handle<> awaiting;

    private:
        AsyncQueue* queue;
    };

private:
    SharedLinkedList<AsyncQueueItem*> waiting_tasks{};
};
}; // namespace lotus
