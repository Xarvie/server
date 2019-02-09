#include "DefConfig.h"
#include <iostream>
#include <vector>
#include <list>
#include <iterator>
#include <thread>
#include <chrono>

#if defined(OS_LINUX)
#include "Spider.h"
#elif defined(OS_DARWIN)
#include "darwinSpider.h"
#endif

#define SPIDER_MAX_NUM 1
int main(int argc, char *const argv[])
{

    Spider xx(9876);
    Msg msg;
    while (true)
    {
        while (true)
        {
            bool ret = xx.msgQueue.try_dequeue(msg);

            if (ret)
                break;
            else
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

        }

        xx.send(msg.fd, msg.buffer.buff, msg.buffer.size);
        msg.buffer.destroy();
        //xx.disconnect(msg.fd);
    }

    std::this_thread::sleep_for(std::chrono::seconds(300));

    return 0;
}
