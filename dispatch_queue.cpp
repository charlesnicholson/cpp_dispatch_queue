#include "dispatch_queue.h"

#include <thread>
#include <queue>
#include <deque>
#include <chrono>
#include <functional>

struct timer_entry
{
    std::chrono::steady_clock::time_point expiry;
    std::function< void() > func;
};

bool operator >(timer_entry const &lhs, timer_entry const &rhs)
{
    return lhs.expiry > rhs.expiry;
}

struct dispatch_queue_t::impl
{
    impl();
    static void dispatch_thread_proc(impl *self);
    static void timer_thread_proc(impl *self);

    std::atomic< bool > quit;

    std::deque< std::function< void() > > queue;
    std::mutex queue_mtx;
    std::condition_variable queue_cond;
    std::thread queue_thread;
    std::atomic< bool > queue_thread_started;
    using queue_lock = std::unique_lock< decltype(queue_mtx) >;

    std::priority_queue< timer_entry, std::vector< timer_entry >, std::greater< timer_entry > > timers;
    std::mutex timer_mtx;
    std::condition_variable timer_cond;
    std::thread timer_thread;
    std::atomic< bool > timer_thread_started;
    using timer_lock = std::unique_lock< decltype(timer_mtx) >;
};

void dispatch_queue_t::impl::dispatch_thread_proc(dispatch_queue_t::impl *self)
{
    queue_lock queue_lock(self->queue_mtx);
    self->queue_cond.notify_one();
    self->queue_thread_started = true;

    while (self->quit == false)
    {
        self->queue_cond.wait(queue_lock, [&] { return !self->queue.empty(); });

        while (!self->queue.empty()) {
            auto dispatch_func = self->queue.back();
            self->queue.pop_back();

            queue_lock.unlock();
            dispatch_func();
            queue_lock.lock();
        }
    }
}

void dispatch_queue_t::impl::timer_thread_proc(dispatch_queue_t::impl *self)
{
    timer_lock timer_lock(self->timer_mtx);
    self->timer_cond.notify_one();
    self->timer_thread_started = true;

    while (self->quit == false)
    {
        self->timer_cond.wait(timer_lock, [&] { return self->quit || !self->timers.empty(); });

        while (!self->timers.empty())
        {
            auto const& timer = self->timers.top();
            if (self->timer_cond.wait_until(timer_lock, timer.expiry, [&] { return self->quit.load(); })) {
                break;
            }

            queue_lock _(self->queue_mtx);
            self->queue.push_back(timer.func);
            self->timers.pop();
            self->queue_cond.notify_one();
        }
    }
}

dispatch_queue_t::impl::impl()
    : quit(false)
    , queue_thread_started(false)
    , timer_thread_started(false)
{
    queue_lock ql(queue_mtx);
    timer_lock tl(timer_mtx);

    queue_thread = std::thread(dispatch_thread_proc, this);
    timer_thread = std::thread(timer_thread_proc, this);

    queue_cond.wait(ql, [this] { return queue_thread_started.load(); });
    timer_cond.wait(tl, [this] { return timer_thread_started.load(); });
}

dispatch_queue_t::dispatch_queue_t()
    : m(new impl)
{
}

dispatch_queue_t::~dispatch_queue_t()
{
    dispatch_async([this] { m->quit = true; });
    m->queue_thread.join();

    {
        impl::timer_lock _(m->timer_mtx);
        m->timer_cond.notify_one();
    }

    m->timer_thread.join();
}

void dispatch_queue_t::dispatch_async(std::function< void() > func)
{
    impl::queue_lock _(m->queue_mtx);
    m->queue.push_front(func);
    m->queue_cond.notify_one();
}

void dispatch_queue_t::dispatch_sync(std::function< void() > func)
{
    std::mutex sync_mtx;
    impl::queue_lock sync_lock(sync_mtx);
    std::condition_variable sync_cond;
    std::atomic< bool > completed(false);

    {
        impl::queue_lock _(m->queue_mtx);
        m->queue.push_front(func);
        m->queue.push_front([&] {
            std::unique_lock< decltype(sync_mtx) > sync_cb_lock(sync_mtx);
            completed = true;
            sync_cond.notify_one();
        });

        m->queue_cond.notify_one();
    }

    sync_cond.wait(sync_lock, [&] { return completed.load(); });
}

void dispatch_queue_t::dispatch_after(int msec, std::function< void() > func)
{
    impl::timer_lock _(m->timer_mtx);
    m->timers.push({ std::chrono::steady_clock::now() + std::chrono::milliseconds(msec), func });
    m->timer_cond.notify_one();
}

void dispatch_queue_t::dispatch_flush()
{
    dispatch_sync([]{});
}

