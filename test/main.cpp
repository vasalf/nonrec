#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <boost/stacktrace.hpp>

#include <functional>
#include <optional>

#include <nonrec.h>

namespace {

nr::nonrec<int> factorial(int n) {
    if (n == 0) {
        co_return 1;
    }
    co_return n * co_await factorial(n - 1);
}

} // namespace <anonymous>

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

namespace {
    
std::string stringify_stacktrace(const boost::stacktrace::stacktrace& stacktrace) {
    std::ostringstream ss;
    ss << stacktrace;
    return ss.str();
}

} // namespace <anonymous>

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
    
    std::function<nr::nonrec<void>(int, bool)> test_void = [&](int depth, bool wide) -> nr::nonrec<void> {
        check_stack_depth();
        if (depth > 0) {
            co_await test_void(depth - 1, wide);
            if (wide) {
                co_await test_void(depth - 1, wide);
            }
        } else {
            reached_end = true;
        }
    };
    
    std::function<nr::nonrec<int>(int, bool)> test_int = [&](int depth, bool wide) -> nr::nonrec<int> {
        int leaves = 0;
        check_stack_depth();
        if (depth > 0) {
            leaves += co_await test_int(depth - 1, wide);
            if (wide) {
                leaves += co_await test_int(depth - 1, wide);
            }
        } else {
            reached_end = true;
            leaves = 1;
        }
        co_return leaves;
    };

    std::optional<std::function<void(int)>> f;
    SUBCASE("return void") {
        bool wide;
        SUBCASE("deep recursion") {
            wide = false;
        }
        SUBCASE("wide recursion") {
            wide = true;
        }
        
        f = [&, wide](int depth) {
            test_void(depth, wide).get();
        };
    }
    SUBCASE("return int") {
        bool wide;
        std::optional<std::function<int(int)>> expected_ret;
        SUBCASE("deep recursion") {
            wide = false;
            expected_ret = [](int) { return 1; };
        }
        SUBCASE("wide recursion") {
            wide = true;
            expected_ret = [](int depth) { return 1 << depth; };
        }
        
        f = [&, wide, expected_ret](int depth) {
            CHECK((*expected_ret)(depth) == test_int(depth, wide).get());
        };
    }

    (*f)(10);
    CHECK(reached_end);
}
