#pragma once

#include <vector>
#include <coroutine>
#include <thread>
#include <memory>
#include <exception>
#include <unordered_map>
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

        SharedLinkedList<Task<>> processing_tasks;
        std::vector<SharedLinkedList<Task<>>> finished_tasks;
        SharedLinkedList<Task<>> deletion_tasks;
        std::exception_ptr exception;

        template<typename... Args>
        Task<> gpuResourceTask(Args&&... args)
        {
            auto a = std::make_tuple<Args...>(std::forward<Args>(args)...);
            co_await std::suspend_always{};
        }

        struct BackgroundTask;

        struct BackgroundPromise
        {
            using coroutine_handle = std::coroutine_handle<BackgroundPromise>;
            auto initial_suspend() noexcept
            {
                return std::suspend_never{};
            }

            struct final_awaitable
            {
                bool await_ready() const noexcept {return false;}
                auto await_suspend(std::coroutine_handle<> handle)
                {
                    WorkerPool::temp_pool->background_tasks.erase(handle.address());
                }
                void await_resume() noexcept {}
            };

            auto final_suspend() noexcept
            {
                return final_awaitable{};
            }

            void unhandled_exception()
            {
                exception = std::current_exception();
            }

            BackgroundTask get_return_object() noexcept;

            void return_void() noexcept {}
            void result()
            {
                if (exception) std::rethrow_exception(exception);
            }

            std::exception_ptr exception;
        };

        struct BackgroundTask
        {
            using promise_type = BackgroundPromise;
            using coroutine_handle = typename promise_type::coroutine_handle;
            BackgroundTask(coroutine_handle _handle) : handle(_handle) {}
            ~BackgroundTask() { if (handle) handle.destroy(); }
            BackgroundTask(const BackgroundTask&) = delete;
            BackgroundTask(BackgroundTask&& o) noexcept : handle(o.handle)
            {
                o.handle = nullptr;
            }
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

        template<Awaitable Task>
        BackgroundTask make_background(Task&& task)
        {
            auto bg_task = std::move(task);
            co_await std::suspend_always{};
            co_await std::move(bg_task);
        }

        //because MSVC messed up the hash for coroutines, we just have to use address manually
        std::unordered_map<void*, BackgroundTask> background_tasks;

        class FrameWait
        {
        public:
            FrameWait(WorkerPool* _pool) : pool(_pool) {}

            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<> awaiter) noexcept
            {
                awaiting = awaiter;
                pool->frame_waiting_tasks.queue(this);
            }
            void await_resume() noexcept {}

        private:
            friend class WorkerPool;
            WorkerPool* pool;
            std::coroutine_handle<> awaiting;
        };
        SharedLinkedList<FrameWait*> frame_waiting_tasks;

    public:

        //move the arguments into a coroutine frame that will be deleted once the GPU is
        // guaranteed to have no need of it anymore (3 frames usually)
        template<typename... Args>
        void gpuResource(Args&&... args)
        {
            auto task = gpuResourceTask(std::forward<Args>(args)...);
            processing_tasks.queue(std::move(task));
        }

        //run the task in the background
        template<Awaitable Task>
        void background(Task&& task)
        {
            auto bg_task = make_background(std::move(task));
            const std::coroutine_handle<> handle = bg_task.handle;
            background_tasks.insert(std::make_pair(handle.address(), std::move(bg_task)));
            handle.resume();
        }

        //suspend the thread until the current frame is done on the CPU side
        [[nodiscard]]
        Task<> waitForFrame() 
        {
            co_await FrameWait(this);
        }

        void processFrameWaits();

        void beginProcessing(size_t image);
        void clearProcessed(size_t image);
        void deleteFinished();
    };

    inline WorkerPool::BackgroundTask WorkerPool::BackgroundPromise::get_return_object() noexcept
    {
        return BackgroundTask{coroutine_handle::from_promise(*this)};
    }
}
