
#include "EventLoop.h"
#include "Util.h"
#include "Logging.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>

class EventLoop {
public:
    using Function=std::function<void()>;
}