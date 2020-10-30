#pragma once

#include <coroutine>
#include <utility>
#include <exception>
#include <iostream>

namespace lotus
{
    //this works when co_await returns the awaiter, but may need something for any other case
    template<typename T>
    using AwaiterType = decltype(std::declval<T>().operator co_await());

    template<typename T>
    using AwaitableReturn = decltype(std::declval<AwaiterType<T>>().await_resume());

    template<typename T>
    concept Awaiter = requires(T t)
    {
        { t.await_ready() };
        { t.await_resume() };
    } && (requires(T t, std::coroutine_handle<> h)
        {
            { t.await_suspend(h) } -> std::convertible_to<bool>;
        } || requires(T t, std::coroutine_handle<> h)
        {
            { t.await_suspend(h) } -> std::convertible_to<std::coroutine_handle<>>;
        } || requires(T t, std::coroutine_handle<> h)
        {
            { t.await_suspend(h) } -> std::same_as<void>;
        }
    );

    template<typename T>
    concept Awaitable = Awaiter<AwaiterType<T>>;

    template<typename Result>
    struct Task;

    struct Promise_base
    {
        auto initial_suspend() noexcept
        {
            return std::suspend_never{};
        }

        struct final_awaitable
        {
            bool await_ready() const noexcept {return false;}
            template<typename Promise>
            auto await_suspend(std::coroutine_handle<Promise> handle)
            {
                return handle.promise().next_handle;
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
        std::coroutine_handle<> next_handle{ std::noop_coroutine() };
    };

    template<typename Result>
    struct Promise : public Promise_base
    {
        using coroutine_handle = std::coroutine_handle<Promise<Result>>;
        Task<Result> get_return_object() noexcept;

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
    struct Promise<void> : public Promise_base
    {
        using coroutine_handle = std::coroutine_handle<Promise<void>>;
        Task<void> get_return_object() noexcept;

        void return_void() noexcept {}
        void result()
        {
            if (exception) std::rethrow_exception(exception);
        }
    };

    template<typename Result = void>
    struct [[nodiscard]] Task
    {
        using promise_type = Promise<Result>;
        using coroutine_handle = typename promise_type::coroutine_handle;
        Task(coroutine_handle _handle) : handle(_handle) {}
        ~Task() { if (handle) handle.destroy(); }
        Task(const Task&) = delete;
        Task(Task&& o) noexcept : handle(o.handle)
        {
            o.handle = nullptr;
        }
        Task& operator=(const Task&) = delete;
        Task& operator=(Task&& o)
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
    Task<Result> Promise<Result>::get_return_object() noexcept
    {
        return Task<Result>{coroutine_handle::from_promise(*this)};
    }

    inline Task<void> Promise<void>::get_return_object() noexcept
    {
        return Task<void>{coroutine_handle::from_promise(*this)};
    }
};
