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
    dq.dispatch_async(hello);
    dq.dispatch_async(std::bind(add, 123, 456));

    {
        auto bound_counter = 0;
        for (auto i = 0; i < 1024; ++i) {
            dq.dispatch_async([&] { ++bound_counter; });
        }

        dq.dispatch_flush();
        std::printf("bound_counter = %d\n", bound_counter);
    }

    std::printf("dispatch_sync sleep for 1s... ");
    std::fflush(stdout);
    dq.dispatch_sync([] { std::this_thread::sleep_for(std::chrono::milliseconds(1000)); });
    std::printf("and we're back.\n");

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

