#include <functional>
#include <memory>

class dispatch_queue_t
{
public:
    dispatch_queue_t();
    ~dispatch_queue_t();

    void dispatch_async(std::function< void() > func);
    void dispatch_sync(std::function< void() > func);
    void dispatch_flush();

    dispatch_queue_t(dispatch_queue_t const &) = delete;
    dispatch_queue_t& operator =(dispatch_queue_t) = delete;

private:
    struct impl;
    std::unique_ptr< impl > m;
};

