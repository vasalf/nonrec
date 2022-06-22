#pragma once

#if __cplusplus < 202002L
#error C++20 support is required.
#endif

#include <coroutine>
#include <functional>
#include <optional>
#include <stack>

namespace nr {

template <typename T>
struct nonrec;

namespace detail {

template <typename T>
struct promise {
    std::optional<T> value;
    std::stack<std::function<void()>>* stack;

    promise() = default;

    auto initial_suspend() {
        return std::suspend_always{};
    }

    auto final_suspend() noexcept {
        return std::suspend_always{};
    }

    nonrec<T> get_return_object();
    void return_value(const T& v);
    void return_value(T&& v);
    void unhandled_exception();
};

template <typename T>
struct awaitable {
    nonrec<T>& handle;

    bool await_ready();

    template <typename S>
    std::coroutine_handle<promise<T>> await_suspend(std::coroutine_handle<promise<S>>& waitee);

    T await_resume();
};

} // namespace detail

template <typename T>
struct nonrec : std::coroutine_handle<detail::promise<T>> {
    using base_type = std::coroutine_handle<detail::promise<T>>;
    using promise_type = detail::promise<T>;

    nonrec(base_type&& handle);

    nonrec(const nonrec&) = delete;
    nonrec(nonrec&&) = delete;
    nonrec& operator=(const nonrec&) = delete;
    nonrec& operator=(nonrec&&) = delete;
    ~nonrec();

    detail::awaitable<T> operator co_await();
    T get() &&;
};

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

template <typename T>
void detail::promise<T>::unhandled_exception() {
    std::terminate();
}

template <typename T>
bool detail::awaitable<T>::await_ready() {
    return false;
}

template <typename T>
template <typename S>
std::coroutine_handle<detail::promise<T>> detail::awaitable<T>::await_suspend(std::coroutine_handle<promise<S>>& waitee) {
    auto *stack = waitee.promise().stack;
    handle.promise().stack = stack;
    stack->push([waitee = &waitee]{ waitee->resume(); });
    return handle;
}

template <typename T>
T detail::awaitable<T>::await_resume() {
    return *handle.promise().value;
}

template <typename T>
nonrec<T>::nonrec(base_type &&handle)
    : base_type{std::move(handle)}
{}

template <typename T>
nonrec<T>::~nonrec() {
    this->destroy();
}

template <typename T>
detail::awaitable<T> nonrec<T>::operator co_await() {
    return {*this};
}

template <typename T>
T nonrec<T>::get() && {
    std::stack<std::function<void()>> stack;
    this->promise().stack = &stack;
    this->resume();
    while (!stack.empty()) {
        auto f = stack.top();
        stack.pop();
        f();
    }
    return *this->promise().value;
}

} // namespace nr
