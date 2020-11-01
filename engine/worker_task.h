#pragma once

#include "engine/task.h"
#include "engine/worker_pool.h"

namespace lotus
{
    template<typename Result = void>
    struct WorkerTask;

    struct WorkerPromise_base
    {
        auto initial_suspend() noexcept
        {
            return WorkerPool::ScheduledTask{WorkerPool::temp_pool};
        }

        struct final_awaitable
        {
            bool await_ready() const noexcept {return false;}
            template<typename Promise>
            auto await_suspend(std::coroutine_handle<Promise> handle)
            {
                return handle.promise().next_handle.exchange(nullptr);
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

        std::exception_ptr exception;
        std::atomic<std::coroutine_handle<>> next_handle{ std::noop_coroutine() };
    };

    template<typename Result>
    struct WorkerPromise : public WorkerPromise_base
    {
        using coroutine_handle = std::coroutine_handle<WorkerPromise<Result>>;
        WorkerTask<Result> get_return_object() noexcept;

        void return_value(Result&& value) noexcept
        {
            result_store = std::move(value);
        }

        Result& result() &
        {
            if (exception) std::rethrow_exception(exception);
            return result_store;
        }

        Result&& result() &&
        {
            if (exception) std::rethrow_exception(exception);
            return std::move(result_store);
        }
    protected:
        Result result_store;
    };

    template<>
    struct WorkerPromise<void> : public WorkerPromise_base
    {
        using coroutine_handle = std::coroutine_handle<WorkerPromise<void>>;
        WorkerTask<void> get_return_object() noexcept;

        void return_void() noexcept {}
        void result()
        {
            if (exception) std::rethrow_exception(exception);
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
                    auto expected = handle.promise().next_handle.load();
                    if (expected && handle.promise().next_handle.compare_exchange_strong(expected, awaiting))
                    {
                        return true;
                    }
                    return false;
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
                    auto expected = handle.promise().next_handle.load();
                    if (expected && handle.promise().next_handle.compare_exchange_strong(expected, awaiting))
                    {
                        return true;
                    }
                    return false;
                }
                auto await_resume()
                {
                    return std::move(handle.promise()).result();
                }
                std::coroutine_handle<promise_type> handle;
            };
            return awaitable{handle};
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

    inline WorkerTask<void> WorkerPromise<void>::get_return_object() noexcept
    {
        return WorkerTask<void>{coroutine_handle::from_promise(*this)};
    }
}