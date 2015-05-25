#include "dispatch_queue.h"

#include <chrono>
#include <thread>
#include <cstdio>

namespace
{
    void hello() { std::printf("hello world\n"); }
    void add(int a, int b) { std::printf("%d + %d = %d\n", a, b, a + b); }
}

int main(void)
{
    dispatch_queue_t dq;

    for (auto i = 0; i < 20; ++i) {
        dq.dispatch_after(i * 50, [=] { std::printf("dispatch_after(%d)\n", i * 50); });
    }

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
    dq.dispatch_sync([] { std::this_thread::sleep_for(std::chrono::milliseconds(1000)); });
    std::printf("dispatch_sync sleep complete.\n");

    {
        auto i = 0;
        dq.dispatch_async([&] {
            std::printf("level %d\n", i++);
            dq.dispatch_async([&] {
                std::printf("level %d\n", i++);
                dq.dispatch_async([&] { std::printf("level %d\n", i++); });
            });
        });
    }

    {
        auto param_counter = 0;
        for (auto i = 0; i < 2048; ++i) {
            dq.dispatch_async(std::bind([](int& c) { ++c; }, std::ref(param_counter)));
        }

        dq.dispatch_flush();
        std::printf("param_counter = %d\n", param_counter);
    }

    return 0;
}

