#include <libdw.h>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <nonrec.h>
#include <libunwind.h>
#include <elfutils/libdwfl.h>
#include <cxxabi.h>

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

std::string demangle(std::string name) {
    int status;
    char *buf = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);
    if (!buf) {
        return name;
    }
    std::string ret{buf};
    free(buf);
    return ret;
}

std::string get_debug_info(unw_cursor_t *cp) {
    struct Dwfl_deleter {
        void operator()(Dwfl *dwfl) {
            dwfl_end(dwfl);
        }
    };

    char* debuginfo_path = nullptr;
    Dwfl_Callbacks callbacks = {
        .find_elf = dwfl_linux_proc_find_elf,
        .find_debuginfo = dwfl_standard_find_debuginfo,
        .debuginfo_path = &debuginfo_path
    };
    std::unique_ptr<Dwfl, Dwfl_deleter> dwfl(dwfl_begin(&callbacks));
    dwfl_linux_proc_report(dwfl.get(), getpid());
    dwfl_report_end(dwfl.get(), nullptr, nullptr);

    unw_word_t unw_ip;
    unw_get_reg(cp, UNW_REG_IP, &unw_ip);
    Dwarf_Addr ip = static_cast<Dwarf_Addr>(unw_ip);

    Dwfl_Module* module = dwfl_addrmodule(dwfl.get(), ip);
    const char* fun = dwfl_module_addrname(module, ip);
    std::ostringstream ret;
    if (fun) {
        ret << demangle(fun);
    } else {
        ret << "[" << ip << "]";
    }

    Dwfl_Line* line_info = dwfl_getsrc(dwfl.get(), ip);
    if (line_info) {
        Dwarf_Addr addr;
        int line;
        const char *filename = dwfl_lineinfo(line_info, &addr, &line, nullptr, nullptr, nullptr);
        ret << " (" << filename << ":" << line << ")";
    }

    return ret.str();
}

std::vector<std::string> get_stacktrace() {
    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local(&cursor, &context);

    std::vector<std::string> ret;
    do {
        ret.emplace_back(get_debug_info(&cursor));
    } while (unw_step(&cursor) > 0);

    return ret;
}

int get_stack_depth() {
    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local(&cursor, &context);

    int ret = 0;
    do {
        ret++;
    } while (unw_step(&cursor) > 0);

    return ret;
}

std::string stringify_stacktrace(std::vector<std::string> stacktrace) {
    std::ostringstream ret;
    ret << std::endl;
    for (const auto& frame : stacktrace) {
        ret << frame << std::endl;
    }
    return ret.str();
}

TEST_CASE("stack depth is constant") {
    bool reached_end = false;
    std::optional<std::vector<std::string>> stacktrace;

    auto check_stack_depth = [&]() {
        if (!stacktrace) {
            stacktrace = get_stacktrace();
        }
        int stacktrace_size = get_stack_depth();
        if (stacktrace->size() != stacktrace_size) {
            auto current_stacktrace = get_stacktrace();
            CAPTURE(stringify_stacktrace(*stacktrace));
            CAPTURE(stringify_stacktrace(current_stacktrace));
            REQUIRE(stacktrace->size() == current_stacktrace.size());
        }
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

    (*f)(5).get();
    CHECK(reached_end);
}
