#include "dispatch_queue.h"

#include <chrono>
#include <cstdio>
#include <thread>

namespace {
void hello() { std::printf("hello world\n"); }
void add(int a, int b) { std::printf("%d + %d = %d\n", a, b, a + b); }
}  // namespace

int main(void) {
    dispatch_queue dq;

    for (auto i = 0; i < 20; ++i) {
        dq.dispatch_after(i * 50,
                          [=] { std::printf("dispatch_after(%d)\n", i * 50); });
    }

    dq.dispatch_after(5, [] { std::printf("explicit dispatch_after(5)\n"); });
    dq.dispatch_after(300,
                      [] { std::printf("explicit dispatch_after(300)\n"); });

    dq.dispatch_async(hello);
    dq.dispatch_async(std::bind(add, 123, 456));

    std::printf("sleeping main thread for 500ms...\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::printf("main thread waking up.\n");

    {
        auto bound_counter = 0;
        for (auto i = 0; i < 1024; ++i) {
            dq.dispatch_async([&] { ++bound_counter; });
        }

        dq.dispatch_flush();
        std::printf("bound_counter = %d\n", bound_counter);
    }

    std::printf("dispatch_sync sleep for 1s...\n");
    std::fflush(stdout);
    dq.dispatch_sync(
        [] { std::this_thread::sleep_for(std::chrono::milliseconds(1000)); });
    std::printf("dispatch_sync sleep complete.\n");

    auto nest = 0;
    {
        dq.dispatch_async([&] {
            std::printf("dispatch_async: nesting level %d\n", nest++);
            dq.dispatch_async([&] {
                std::printf("dispatch_async: nesting level %d\n", nest++);
                dq.dispatch_async([&] {
                    std::printf("dispatch_async: nesting level %d\n", nest++);
                });
            });
        });
    }

    {
        auto param_counter = 0;
        for (auto i = 0; i < 2048; ++i) {
            dq.dispatch_async(
                std::bind([](int& c) { ++c; }, std::ref(param_counter)));
        }

        dq.dispatch_flush();
        std::printf("param_counter = %d\n", param_counter);
    }

    return 0;
}

