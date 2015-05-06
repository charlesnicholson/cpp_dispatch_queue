#include "dispatch_queue.h"

#include <thread>
#include <queue>

struct dispatch_queue_t::impl
{
    impl();
    static void dispatch_thread_proc(impl *self);

    std::queue< std::function< void() > > queue;
    std::mutex queue_mtx;
    std::condition_variable queue_cond;
    std::atomic< bool > quit;
    std::thread queue_thread;

    using queue_lock_t = std::unique_lock< decltype(queue_mtx) >;
};

void dispatch_queue_t::impl::dispatch_thread_proc(dispatch_queue_t::impl *self)
{
    using queue_size_t = decltype(self->queue)::size_type;

    while (self->quit == false)
    {
        queue_lock_t queue_lock(self->queue_mtx);
        queue_size_t n = self->queue.size();

        self->queue_cond.wait(queue_lock, [&self]() { return self->queue.size() > 0; });

        for (queue_size_t i = 0; i < n; ++i) {
            auto dispatch_func = self->queue.front();
            self->queue.pop();

            queue_lock.unlock();
            dispatch_func();
            queue_lock.lock();
        }
    }
}

dispatch_queue_t::impl::impl()
    : quit(false)
    , queue_thread(&dispatch_thread_proc, this)
{
}

dispatch_queue_t::dispatch_queue_t()
    : m(new impl)
{
}

dispatch_queue_t::~dispatch_queue_t()
{
    dispatch_async([this] { m->quit = true; });
    m->queue_thread.join();
}

void dispatch_queue_t::dispatch_async(std::function< void() > func)
{
    impl::queue_lock_t queue_lock(m->queue_mtx);
    m->queue.push(func);
    m->queue_cond.notify_one();
}

void dispatch_queue_t::dispatch_sync(std::function< void() > func)
{
    std::mutex sync_mtx;
    impl::queue_lock_t sync_lock(sync_mtx);
    std::condition_variable sync_cond;
    auto completed = false;

    {
        impl::queue_lock_t queue_lock(m->queue_mtx);
        m->queue.push(func);
        m->queue.push([&]() {
            std::unique_lock< decltype(sync_mtx) > sync_cb_lock(sync_mtx);
            completed = true;
            sync_cond.notify_one();
        });

        m->queue_cond.notify_one();
    }

    sync_cond.wait(sync_lock, [&completed]() { return completed; });
}

void dispatch_queue_t::dispatch_flush()
{
    dispatch_sync([]{});
}

