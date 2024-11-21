#pragma once

#include "async_queue.h"
#include "renderer/vulkan/vulkan_inc.h"
#include "shared_linked_list.h"
#include "task.h"
#include <coroutine>
#include <exception>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

namespace lotus
{
class Engine;

class WorkerPool
{
public:
    WorkerPool(Engine*);

    ~WorkerPool() { Stop(); }

    // temporary workaround for WorkerPromise's issues
    static inline WorkerPool* temp_pool;

    class ScheduledTask
    {
    public:
        // queue when suspended
        ScheduledTask(WorkerPool* _pool) : pool(_pool) {}
        // queue an existing coroutine handle (must call queueTask after making sure the scheduledTask object is
        // constructed
        //  or else it might run before finishing construction/assignment)
        ScheduledTask(WorkerPool* _pool, std::coroutine_handle<> _awaiting) : pool(_pool), awaiting(_awaiting) {}
        void queueTask() { pool->queueTask(this); }

        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> awaiter) noexcept
        {
            awaiting = awaiter;
            pool->queueTask(this);
        }
        void await_resume() noexcept {}

    private:
        friend class WorkerPool;
        WorkerPool* pool;
        ScheduledTask* next{nullptr};
        std::coroutine_handle<> awaiting;
    };

    struct CommandBuffers
    {
        SharedLinkedList<vk::CommandBuffer> graphics_primary;
        SharedLinkedList<vk::CommandBuffer> graphics_secondary;
        SharedLinkedList<vk::CommandBuffer> shadowmap;
        SharedLinkedList<vk::CommandBuffer> particle;
    } command_buffers;

    std::vector<vk::CommandBuffer> getPrimaryGraphicsBuffers(int);
    std::vector<vk::CommandBuffer> getSecondaryGraphicsBuffers(int);
    std::vector<vk::CommandBuffer> getShadowmapGraphicsBuffers(int);
    std::vector<vk::CommandBuffer> getParticleGraphicsBuffers(int);

    void Reset();

    // called by the main thread to wait until the game exits
    void Run();

    void Stop();
    void Stop(std::exception_ptr);

    class MainThreadTask
    {
    public:
        MainThreadTask(WorkerPool* _pool) : pool(_pool) {}

        bool await_ready() noexcept { return std::this_thread::get_id() == pool->main_thread; }
        void await_suspend(std::coroutine_handle<> awaiter) noexcept
        {
            awaiting = awaiter;
            pool->main_task = this;
            pool->worker_flag.clear();
            pool->main_flag.test_and_set();
            pool->main_flag.notify_one();
        }
        void await_resume() noexcept {}

    private:
        friend class WorkerPool;
        WorkerPool* pool;
        std::coroutine_handle<> awaiting;
    };

    [[nodiscard]]
    MainThreadTask mainThread()
    {
        return MainThreadTask{this};
    }
    void queueTask(ScheduledTask*);

private:
    // friend class ScheduledTask;
    friend class MainThreadTask;
    ScheduledTask* tryGetTask();
    void runTasks(std::stop_token);

    Engine* engine;
    std::vector<std::jthread> threads;
    std::atomic<ScheduledTask*> task_head{nullptr};

    // main thread synchronization:
    std::thread::id main_thread{std::this_thread::get_id()};
    MainThreadTask* main_task{nullptr};
    std::atomic_flag main_flag;
    std::atomic_flag worker_flag;

    SharedLinkedList<Task<>> processing_tasks;
    std::vector<SharedLinkedList<Task<>>> finished_tasks;
    SharedLinkedList<Task<>> deletion_tasks;
    std::exception_ptr exception;

    template <typename... Args> Task<> gpuResourceTask(Args&&... args)
    {
        auto a = std::make_tuple<Args...>(std::forward<Args>(args)...);
        co_await std::suspend_always{};
    }

    struct BackgroundTask;

    struct BackgroundPromise
    {
        using coroutine_handle = std::coroutine_handle<BackgroundPromise>;
        auto initial_suspend() noexcept { return std::suspend_never{}; }

        struct final_awaitable
        {
            bool await_ready() const noexcept { return false; }
            auto await_suspend(std::coroutine_handle<> handle) noexcept { WorkerPool::temp_pool->background_tasks.erase(handle); }
            void await_resume() noexcept {}
        };

        auto final_suspend() noexcept { return final_awaitable{}; }

        void unhandled_exception() { exception = std::current_exception(); }

        BackgroundTask get_return_object() noexcept;

        void return_void() noexcept {}
        void result()
        {
            if (exception)
                std::rethrow_exception(exception);
        }

        std::exception_ptr exception;
    };

    struct BackgroundTask
    {
        using promise_type = BackgroundPromise;
        using coroutine_handle = typename promise_type::coroutine_handle;
        BackgroundTask(coroutine_handle _handle) : handle(_handle) {}
        ~BackgroundTask()
        {
            if (handle)
                handle.destroy();
        }
        BackgroundTask(const BackgroundTask&) = delete;
        BackgroundTask(BackgroundTask&& o) noexcept : handle(o.handle) { o.handle = nullptr; }
        BackgroundTask& operator=(const BackgroundTask&) = delete;
        BackgroundTask& operator=(BackgroundTask&& o)
        {
            if (std::addressof(o) != this)
            {
                if (handle)
                    handle.destroy();

                handle = o.handle;
                o.handle = nullptr;
            }
            return *this;
        }

    protected:
        friend class WorkerPool;
        coroutine_handle handle;
    };

    template <Awaitable Task> BackgroundTask make_background(Task&& task)
    {
        auto bg_task = std::move(task);
        co_await std::suspend_always{};
        co_await std::move(bg_task);
    }

    std::unordered_map<std::coroutine_handle<>, BackgroundTask> background_tasks;

    AsyncQueue<> frame_waiting_queue;

public:
    // move the arguments into a coroutine frame that will be deleted once the GPU is
    //  guaranteed to have no need of it anymore (3 frames usually)
    template <typename... Args> void gpuResource(Args&&... args)
    {
        auto task = gpuResourceTask(std::forward<Args>(args)...);
        processing_tasks.queue(std::move(task));
    }

    // run the task in the background
    template <Awaitable Task> void background(Task&& task)
    {
        auto bg_task = make_background(std::move(task));
        const std::coroutine_handle<> handle = bg_task.handle;
        background_tasks.insert(std::make_pair(handle, std::move(bg_task)));
        handle.resume();
    }

    // suspend the thread until the current frame is done on the CPU side
    [[nodiscard]]
    Task<> waitForFrame()
    {
        co_await frame_waiting_queue.wait();
    }

    void processFrameWaits();

    void beginProcessing(size_t image);
    void clearProcessed(size_t image);
    void deleteFinished();
};

inline WorkerPool::BackgroundTask WorkerPool::BackgroundPromise::get_return_object() noexcept { return BackgroundTask{coroutine_handle::from_promise(*this)}; }
} // namespace lotus
