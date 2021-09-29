#pragma once

#include <coroutine>
#include "shared_linked_list.h"

namespace lotus
{
    template<typename T = void>
    class AsyncQueue
    {
    public:
        auto wait(T&& data) { return AsyncQueueItem(this, std::forward<T&&>(data)); }
        auto getAll() { return waiting_tasks.getAll(); }
    private:
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
            auto await_resume() noexcept
            {
                return data;
            }

            std::coroutine_handle<> awaiting;
            T data;
        private:
            AsyncQueue* queue;
        };
        SharedLinkedList<AsyncQueueItem*> waiting_tasks{};
    };

    template<>
    class AsyncQueue<void>
    {
    public:
        auto wait() { return AsyncQueueItem(this); }
        auto getAll() { return waiting_tasks.getAll(); }
    private:
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
        SharedLinkedList<AsyncQueueItem*> waiting_tasks{};
    };
};

