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
            if (joinable())
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
            return MainThreadTask{ this };
        }

    private:
        friend class ScheduledTask;
        friend class MainThreadTask;
        ScheduledTask* tryGetTask();
        ScheduledTask* getTask();
        void runTasks(std::stop_token);
        void queueTask(ScheduledTask*);
        void Wait();

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
}
