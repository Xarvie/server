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
//std::vector<Spider> spiders;
#define SPIDER_MAX_NUM 1
int main(int argc, char * const argv[])
{
	Spider* x;
	//x = new Spider();
	//delete  x;
	{

		Spider  xx(9876);
		Msg msg;

		/**
		 * xx.listen(9876, 4);
		 * //int fd = xx.connect("192.168.18.46", 8080);
		 * //xx.send(fd, "halo");
		 * while(true)
		 * {
		 *     Msg msg;
		 *     while(true)
		 *     {
		 *         bool ret = xx.get(msg);
		 *         if(ret)
		 *             break;
		 *         else
		 *             std::this_thread::sleep_for(std::chrono::milliseconds(10));
		 *
		 *     }
		 *
		 *     xx.send(msg.fd, msg.buffer.buff, msg.buffer.size);
		 *     xx.disconnect(msg.fd);
		 * }
		 *
		 */
	}
	std::this_thread::sleep_for(std::chrono::seconds(30));

	return 0;
}
