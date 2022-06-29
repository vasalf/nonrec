#include <boost/stacktrace/stacktrace_fwd.hpp>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <boost/stacktrace.hpp>

#include <nonrec.h>

nr::nonrec<int> factorial(int n) {
    if (n == 0) {
        co_return 1;
    }
    co_return n * co_await factorial(n - 1);
}

TEST_CASE("smoke: factorial") {
    for (int n = 0, fct = 1; n <= 10; n++) {
        CHECK(factorial(n).get() == fct);
        fct *= n + 1;
    }
}

TEST_CASE("smoke: fibonacci") {
    std::function<nr::nonrec<int>(int)> fibonacci = [&](int n) -> nr::nonrec<int> {
        if (n <= 1) {
            co_return n;
        }
        co_return co_await fibonacci(n - 1) + co_await fibonacci(n - 2);
    };

    for (int n = 0, a = 0, b = 1; n <= 10; n++) {
        CHECK(fibonacci(n).get() == a);
        std::tie(a, b) = std::make_pair(b, a + b);
    }
}

TEST_CASE("smoke: void return type") {
    bool reached_end = false;

    std::function<nr::nonrec<void>(int)> f = [&](int depth) -> nr::nonrec<void> {
        if (depth == 0) {
            reached_end = true;
            co_return;
        }
        co_await f(depth - 1);
    };

    f(10).get();
    CHECK(reached_end);
}

std::string stringify_stacktrace(const boost::stacktrace::stacktrace& stacktrace) {
    std::ostringstream ss;
    ss << stacktrace;
    return ss.str();
}

TEST_CASE("stack depth is constant") {
    bool reached_end = false;
    std::optional<boost::stacktrace::stacktrace> stacktrace;

    auto check_stack_depth = [&]() {
        auto current_stacktrace = boost::stacktrace::stacktrace();
        if (!stacktrace) {
            stacktrace = current_stacktrace;
        }
        CAPTURE(stringify_stacktrace(*stacktrace));
        CAPTURE(stringify_stacktrace(current_stacktrace));
        REQUIRE(stacktrace->size() == current_stacktrace.size());
    };

    std::optional<std::function<nr::nonrec<void>(int)>> f;
    SUBCASE("deep recursion") {
        f = [&](int depth) -> nr::nonrec<void> {
            check_stack_depth();
            if (depth > 0) {
                co_await (*f)(depth - 1);
            } else {
                reached_end = true;
            }
        };
    }
    SUBCASE("wide recursion") {
        f = [&](int depth) -> nr::nonrec<void> {
            check_stack_depth();
            if (depth > 0) {
                co_await (*f)(depth - 1);
                co_await (*f)(depth - 1);
            } else {
                reached_end = true;
            }
        };
    }

    (*f)(10).get();
    CHECK(reached_end);
}
