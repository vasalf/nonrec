# `nr::nonrec`

![Unit Tests](https://github.com/vasalf/nonrec/actions/workflows/unit-tests.yml/badge.svg)

A C++20 one header library to turn recursive functions into non-recursive.

## Example

Suppose you have a (very inefficient) routine you use to calculate the factorial of a number modulo another number:

```C++
int factorial(int n, int mod) {
    if (n == 0) { 
        return 1;
    }
    int prev = factorial(n - 1, mod);
    return (static_cast<long long>(n) * prev) % mod;
}
```

With nonrec you can rewrite is as

```C++
#include <nonrec.h>

nr::nonrec<int> factorial(int n, int mod) {
    if (n == 0) {
        co_return 1;
    }
    int prev = co_await factorial(n - 1, mod);
    co_return (static_cast<long long>(n) * prev) % mod;
}
```

Now, if you call it as

```C++
int fct = factorial(1'000'000, 179).get();
```

The factorial will be calculated using a heap-allocated dynamic stack, without causing a stack overflow runtime error.

### Lambda functions

Using `nonrec` within lambda functions is possible, though requires a somewhat more explicit syntax:

```C++
#include <nonrec.h>

#include <iostream>
#include <functional>

int main() {
    std::function<nr::nonrec<int>(int, int)> factorial = [&](int n, int mod) -> nr::nonrec<int> {
        if (n == 0) {
            co_return 1;
        }
        int prev = co_await factorial(n - 1, mod);
        co_return (static_cast<long long>(n) * prev) % mod;
    };
    std::cout << factorial(1'000'000, 179).get() << std::endl;
}
```

## Behind the scenes 

The `nr::nonrec<T>` type is a [coroutine](https://en.cppreference.com/w/cpp/language/coroutines) handle. Coroutine is, roughly, a function whose execution can be stopped at arbitrary moment and then resumed. When the execution reaches the `co_await` statement, it stops the coroutine and returns the control to the `.get()` function, which places the waiting and awaited coroutines on top of the internal stack and continues to execute the coroutine from the stack top (which is, in this case, the awaited one). When the execution reaches the co_return statement, it saves the returned value and returns the control to the `.get()` function, which, in turn, feeds the returned value to the coroutine from the top of the stack (which is the waiting coroutine in this case) and continues its execution.

To sum up, the recursive execution is emulated on a heap-allocated stack, and the C++20 coroutines syntax allows to rewrite those functions without changing the code structure.

## Installation

nonrec is a one-header library with no dependencies except for STL. Simply copy the [include/nonrec.h](include/nonrec.h) file to your include path, and you are good to go.

## Limitations

1. nonrec requires C++20 coroutines to work, which, in order, requires a fairly new compiler. It has been tested against GCC >= 11, LLVM >= 13, MSVC >= 19 and Apple Clang >= 13. It also requires C++20 standard to be enabled.
2. Executing frequently stopping coroutines is fairly slow. You can expect a 7-10x overhead on call/return actions, depending on the compiler. You can consult the `Benchmark` task of the latest GitHub Actions build to see the results for your compiler.
3. At this point, uncaught exceptions within the functions will cause `std::terminate`.
