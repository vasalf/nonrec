#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <nonrec.h>
#include <libunwind.h>

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

int get_stack_depth() {
    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local(&cursor, &context);

    int ret = 0;
    while (unw_step(&cursor) > 0) {
        ret++;
    }

    return ret;
}

TEST_CASE("stack depth is constant") {
    std::optional<int> stack_depth;

    auto check_stack_depth = [&]() {
        int current_stack_depth = get_stack_depth();
        if (!stack_depth) {
            stack_depth = current_stack_depth;
        }
        CHECK(*stack_depth == current_stack_depth);
    };

    std::optional<std::function<nr::nonrec<int>(int)>> f;
    SUBCASE("deep recursion") {
        f = [&](int depth) -> nr::nonrec<int> {
            check_stack_depth();
            if (depth > 0) {
                co_await (*f)(depth - 1);
            }
            co_return 0; // TODO: support void return type
        };
    }
    SUBCASE("wide recursion") {
        f = [&](int depth) -> nr::nonrec<int> {
            check_stack_depth();
            if (depth > 0) {
                co_await (*f)(depth - 1);
                co_await (*f)(depth - 1);
            }
            co_return 0; // TODO: support void return type
        };
    }

    (*f)(10).get();
}
