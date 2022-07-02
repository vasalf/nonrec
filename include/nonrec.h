// nonrec, a C++20 library to turn recursive functions into non-recursive.
//
// Copyright (c) 2022 Vasily Alferov
// 
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#if defined(_MSC_VER) && __cplusplus == 199711L

#pragma message("WARNING: Unable to determine C++ standard version. Consider adding" \
                " /Zc:__cplusplus to the compiler flags. See" \
                " https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/" \
                " for more information.")
                
#elif __cplusplus < 202002L

#error C++20 support is required.

#endif

#if defined(__clang__) && !defined(_LIBCPP_VERSION)
#error libc++ required for coroutines in clang to work
#endif

#if defined(__clang__) && __clang_major__ < 14

#include <experimental/coroutine>
namespace coro = std::experimental;

#else

#include <coroutine>
namespace coro = std;

#endif

#include <optional>
#include <stack>
#include <type_traits>

namespace nr {

template <typename T>
struct nonrec;

namespace detail {

struct suspend_if {
    bool condition;

    bool await_ready() {
        return !condition;
    }

    void await_suspend(coro::coroutine_handle<>) {}
    void await_resume() {}
};

struct promise_base {
    std::stack<coro::coroutine_handle<>>* stack = nullptr;

    promise_base() = default;

    auto initial_suspend() {
        return suspend_if{stack == nullptr};
    }

    template <typename S>
    nonrec<S> await_transform(nonrec<S> expr);

    void unhandled_exception() {
        std::terminate();
    }
};

template <typename T>
struct promise : promise_base {
    std::optional<T> value;

    promise() = default;

    auto final_suspend() noexcept {
        return coro::suspend_always{};
    }

    nonrec<T> get_return_object();
    void return_value(const T& v);
    void return_value(T&& v);
};

template <>
struct promise<void> : promise_base {
    promise() = default;

    auto final_suspend() noexcept {
        return coro::suspend_never{};
    }

    inline nonrec<void> get_return_object();
    void return_void() {}
};

template <typename T>
struct awaitable {
    nonrec<T>& handle;

    bool await_ready();

    template <typename S>
    void await_suspend(coro::coroutine_handle<promise<S>> waitee);

    T await_resume();
};

} // namespace detail

template <typename T>
struct nonrec : coro::coroutine_handle<detail::promise<T>> {
    using base_type = coro::coroutine_handle<detail::promise<T>>;
    using promise_type = detail::promise<T>;

    nonrec(base_type&& handle);
    nonrec(nonrec&& other);

    nonrec(const nonrec&) = delete;
    nonrec& operator=(const nonrec&) = delete;
    nonrec& operator=(nonrec&&) = delete;
    ~nonrec();

    detail::awaitable<T> operator co_await();
    T get() &&;

private:
    bool alive_;
};

template <typename S>
nonrec<S> detail::promise_base::await_transform(nonrec<S> expr) {
    expr.promise().stack = stack;
    return expr;
}

template <typename T>
nonrec<T> detail::promise<T>::get_return_object() {
    return nonrec<T>::from_promise(*this);
}

template <typename T>
void detail::promise<T>::return_value(const T &v) {
    value.emplace(v);
}

template <typename T>
void detail::promise<T>::return_value(T&& v) {
    value.emplace(std::move(v));
}

inline nonrec<void> detail::promise<void>::get_return_object() {
    return nonrec<void>::from_promise(*this);
}

template <typename T>
bool detail::awaitable<T>::await_ready() {
    if constexpr (!std::is_void_v<T>) {
        return handle.promise().value.has_value();
    }
    return handle.done();
}

template <typename T>
template <typename S>
void detail::awaitable<T>::await_suspend(coro::coroutine_handle<promise<S>> waitee) {
    handle.promise().stack->push(waitee);
    handle.promise().stack->push(handle);
}

template <typename T>
T detail::awaitable<T>::await_resume() {
    if constexpr (!std::is_void_v<T>) {
        return *handle.promise().value;
    }
}

template <typename T>
nonrec<T>::nonrec(base_type&& handle)
    : base_type{std::move(handle)}
    , alive_(true)
{}

template <typename T>
nonrec<T>::nonrec(nonrec&& other)
    : base_type{std::move(other)}
    , alive_(std::exchange(other.alive_, false))
{}

template <typename T>
nonrec<T>::~nonrec() {
    if constexpr (!std::is_void_v<T>) {
        if (alive_) {
            this->destroy();
        }
    }
}

template <typename T>
detail::awaitable<T> nonrec<T>::operator co_await() {
    return {*this};
}

template <typename T>
T nonrec<T>::get() && {
    std::stack<coro::coroutine_handle<>> stack;
    this->promise().stack = &stack;
    this->resume();
    while (!stack.empty()) {
        auto next_handle = stack.top();
        stack.pop();
        next_handle.resume();
    }
    if constexpr (!std::is_void_v<T>) {
        return *this->promise().value;
    }
}

} // namespace nr
