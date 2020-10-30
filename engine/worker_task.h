#pragma once

#include "engine/task.h"
#include "engine/worker_pool.h"

namespace lotus
{
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
                auto await_suspend(std::coroutine_handle<> awaiting) noexcept
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
                auto await_suspend(std::coroutine_handle<> awaiting) noexcept
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