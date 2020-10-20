#pragma once

#include <vector>
#include <coroutine>
#include <condition_variable>
#include <ranges>
#include <thread>
#include <memory>
#include <exception>
#include "task.h"
#include "shared_linked_list.h"
#include "renderer/vulkan/vulkan_inc.h"

#ifdef _MSC_VER
namespace std
{
    //bootleg jthread until MSVC implements it :)
    class jthread;
    class stop_token
    {
        friend class jthread;
        shared_ptr<bool> stop{ make_shared<bool>(false) };
    public:
        bool stop_requested() { return *stop; }
    };
    class jthread : public thread
    {
    public:
        jthread() noexcept : thread() {}
        template <typename Fn, typename... Args>
        explicit jthread(Fn&& fn, Args&&... args) : jthread(forward<Fn>(fn), stop_token{}, forward<Args>(args)...) {}
        template <typename Fn, typename... Args>
        explicit jthread(Fn&& fn, stop_token stop, Args&&... args) : thread(forward<Fn>(fn), stop, forward<Args>(args)...), stop{ stop } {}

        jthread(jthread&&) = default;

        jthread& operator=(jthread&&) = default;

        jthread(const jthread&) = delete;
        jthread& operator=(const jthread&) = delete;

        void request_stop() { *stop.stop = true; }
        stop_token get_stop_token() { return stop; }

        ~jthread() noexcept {
            join();
        }
    private:
        stop_token stop;
    };
}
#endif

namespace lotus
{
    class Engine;

    template<typename Result = void>
    struct WorkerTask;

    class WorkerPool
    {
    public:
        WorkerPool(Engine*);

        //temporary workaround for WorkerPromise's issues
        static inline WorkerPool* temp_pool;

        class ScheduledTask
        {
        public:
            ScheduledTask(WorkerPool* _pool) : pool(_pool) {}

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
            ScheduledTask* next {nullptr};
            std::coroutine_handle<> awaiting;
        };

        struct CommandBuffers
        {
            SharedLinkedList<vk::CommandBuffer> graphics_primary;
            SharedLinkedList<vk::CommandBuffer> graphics_secondary;
            SharedLinkedList<vk::CommandBuffer> shadowmap;
            SharedLinkedList<vk::CommandBuffer> particle;
            SharedLinkedList<vk::CommandBuffer> compute;
        } command_buffers;

        std::vector<vk::CommandBuffer> getPrimaryGraphicsBuffers(int);
        std::vector<vk::CommandBuffer> getSecondaryGraphicsBuffers(int);
        std::vector<vk::CommandBuffer> getShadowmapGraphicsBuffers(int);
        std::vector<vk::CommandBuffer> getParticleGraphicsBuffers(int);
        std::vector<vk::CommandBuffer> getPrimaryComputeBuffers(int);

        //TODO
        void reset() {}

        //called by the main thread to wait until the game exits
        void Wait(WorkerTask<> mainLoop);
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
            return MainThreadTask{ this };
        }

    private:
        friend class ScheduledTask;
        friend class MainThreadTask;
        ScheduledTask* tryGetTask();
        ScheduledTask* getTask();
        void runTasks(std::stop_token);
        void queueTask(ScheduledTask*);

        Engine* engine;
        std::vector<std::jthread> threads;
        std::atomic<ScheduledTask*> task_head{nullptr};

        //main thread synchronization:
        std::thread::id main_thread{ std::this_thread::get_id() };
        MainThreadTask* main_task{ nullptr };
        std::atomic_flag main_flag;
        std::atomic_flag worker_flag;

        //TODO: this should be a shared_ptr specialization of std::atomic, but it doesn't exist yet in gcc
        SharedLinkedList<Task<>> processing_tasks;
        std::vector<SharedLinkedList<Task<>>> finished_tasks;
        SharedLinkedList<Task<>> deletion_tasks;
        std::exception_ptr exception;

        template<typename... Args>
        Task<> frameQueueTask(Args&&... args)
        {
            auto a = std::make_tuple<Args...>(std::forward<Args>(args)...);
            co_await std::suspend_always{};
        }
    public:

        template<typename... Args>
        void frameQueue(Args&&... args)
        {
            auto task = frameQueueTask(std::forward<Args>(args)...);
            processing_tasks.queue(std::move(task));
        }

        void beginProcessing(size_t image)
        {
            finished_tasks[image] = std::move(processing_tasks);
        }

        void clearProcessed(size_t image)
        {
            deletion_tasks = std::move(finished_tasks[image]);
        }

        void deleteFinished()
        {
            deletion_tasks = {};
        }
    };

    template<typename Result>
    struct WorkerPromise : public Promise<Result>
    {
        //TODO: this doesn't work until GCC supports p0914
        // workaround is the operator new overload - remove when support is added (or when I move to clang)
//        template<typename... Args>
//        WorkerPromise(WorkerPool* _pool, Args&&...) : pool(_pool) {}
        //nevermind operator new doesn't work either, gcc is a meme
        /*
        template<typename... Args>
        void* operator new(size_t sz, WorkerPool* _pool, Args const&...)
        {
            saved_pool = _pool;
            return ::operator new(sz);
        }*/
        using coroutine_handle = std::coroutine_handle<WorkerPromise<Result>>;
        WorkerTask<Result> get_return_object() noexcept;
        auto initial_suspend() noexcept
        {
            return WorkerPool::ScheduledTask{WorkerPool::temp_pool};
        }
    };

    template<typename Result>
    struct WorkerTask
    {
        using promise_type = WorkerPromise<Result>;
        using coroutine_handle = typename promise_type::coroutine_handle;
        WorkerTask(coroutine_handle _handle) : handle(_handle) {}
        ~WorkerTask() { if (handle) handle.destroy(); }
        WorkerTask(const WorkerTask&) = delete;
        WorkerTask(WorkerTask&& o) noexcept : handle(o.handle)
        {
            o.handle = nullptr;
        }
        WorkerTask& operator=(const WorkerTask&) = delete;
        WorkerTask& operator=(WorkerTask&& o)
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

        auto operator co_await() const & noexcept
        {
            struct awaitable
            {
                awaitable(std::coroutine_handle<promise_type> _handle) : handle(_handle) {}
                auto await_ready() const noexcept
                {
                    return !handle || handle.done();
                }
                void await_suspend(std::coroutine_handle<> awaiting) noexcept
                {
                    handle.promise().next_handle = awaiting;
                }
                auto await_resume()
                {
                    return handle.promise().result();
                }
                std::coroutine_handle<promise_type> handle;
            };
            return awaitable{handle};
        }

        auto operator co_await() const && noexcept
        {
            struct awaitable
            {
                awaitable(std::coroutine_handle<promise_type> _handle) : handle(_handle) {}
                auto await_ready() const noexcept
                {
                    return !handle || handle.done();
                }
                void await_suspend(std::coroutine_handle<> awaiting) noexcept
                {
                    handle.promise().next_handle = awaiting;
                }
                auto await_resume()
                {
                    return std::move(handle.promise()).result();
                }
                std::coroutine_handle<promise_type> handle;
            };
            return awaitable{handle};
        }

        auto result()
        {
            return handle.promise().result();
        }

//    protected:
        friend class WorkerPool;
        coroutine_handle handle;
    };

    template<typename Result>
    WorkerTask<Result> WorkerPromise<Result>::get_return_object() noexcept
    {
        return WorkerTask<Result>{coroutine_handle::from_promise(*this)};
    }

}
