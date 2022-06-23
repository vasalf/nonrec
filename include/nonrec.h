#pragma once

#if __cplusplus < 202002L
#error C++20 support is required.
#endif

#include <coroutine>
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

    void await_suspend(std::coroutine_handle<>) {}
    void await_resume() {}
};

struct promise_base {
    std::stack<std::coroutine_handle<>>* stack = nullptr;

    promise_base() = default;

    auto initial_suspend() {
        return suspend_if{stack == nullptr};
    }

    template <typename S>
    nonrec<S> await_transform(nonrec<S> expr);

    void unhandled_exception();
};

template <typename T>
struct promise : promise_base {
    std::optional<T> value;

    promise() = default;

    auto final_suspend() noexcept {
        return std::suspend_always{};
    }

    nonrec<T> get_return_object();
    void return_value(const T& v);
    void return_value(T&& v);
};

template <>
struct promise<void> : promise_base {
    promise() = default;

    auto final_suspend() noexcept {
        return std::suspend_never{};
    }

    nonrec<void> get_return_object();
    void return_void() {}
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

void detail::promise_base::unhandled_exception() {
    std::terminate();
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

nonrec<void> detail::promise<void>::get_return_object() {
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
std::coroutine_handle<detail::promise<T>> detail::awaitable<T>::await_suspend(std::coroutine_handle<promise<S>>& waitee) {
    handle.promise().stack->push(waitee);
    return handle;
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
    std::stack<std::coroutine_handle<>> stack;
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
