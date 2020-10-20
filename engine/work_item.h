#pragma once
#include <functional>
#include <coroutine>
#include <engine/renderer/vulkan/vulkan_inc.h>
#include "engine/task.h"

namespace lotus
{
    class Engine;

    /*
    template<typename Result>
    struct WorkItemTask;

    struct WorkItemTaskPromise_base
    {
        auto initial_suspend() noexcept
        {
            return std::suspend_always{};
        }

        auto final_suspend() noexcept
        {
            return std::suspend_always{};
        }

        void return_void() {}

        void unhandled_exception() {}

        std::coroutine_handle<> next_handle;
    };

    template<typename Result>
    struct WorkItemTaskPromise : public WorkItemTaskPromise_base
    {
        using coroutine_handle = std::coroutine_handle<WorkItemTaskPromise<Result>>;
        WorkItemTask<Result> get_return_object() noexcept;

        struct yield_awaitable
        {
            auto await_ready() const noexcept {return false;}
            template<typename Promise>
            auto await_suspend(std::coroutine_handle<Promise> handle)
            {
                if (handle.promise().next_handle)
                    handle.promise().next_handle.resume();
            }
            void await_resume() noexcept {}
        };

        auto yield_value(Result&& r)
        {
            result_store = std::move(r);
            return yield_awaitable{};
        }

        Result& result() &
        {
            return result_store;
        }

        Result&& result() &&
        {
            return std::move(result_store);
        }

    private:
        Result result_store;
    };

    template<>
    struct WorkItemTaskPromise<void> : public WorkItemTaskPromise_base
    {
        using coroutine_handle = std::coroutine_handle<WorkItemTaskPromise<void>>;
        WorkItemTask<void> get_return_object() noexcept;

        auto yield_value() noexcept
        {
            return std::suspend_always{};
        }

        void result() noexcept {}
    };

    template<typename Result = void>
    struct WorkItemTask
    {
        using promise_type = WorkItemTaskPromise<Result>;
        using coroutine_handle = typename promise_type::coroutine_handle;
        WorkItemTask(coroutine_handle _handle) : handle(_handle) {}
        ~WorkItemTask() { if (handle) handle.destroy(); }
        WorkItemTask(const WorkItemTask&) = delete;
        WorkItemTask(WorkItemTask&& o) noexcept : handle(o.handle)
        {
            o.handle = nullptr;
        }
        WorkItemTask& operator=(const WorkItemTask&) = delete;
        WorkItemTask& operator=(WorkItemTask&& o)
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
                    return handle;
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
                    return handle;
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

    protected:
        coroutine_handle handle;
    };

    template<typename Result>
    WorkItemTask<Result> WorkItemTaskPromise<Result>::get_return_object() noexcept
    {
        return WorkItemTask<Result>{coroutine_handle::from_promise(*this)};
    }*/
}
