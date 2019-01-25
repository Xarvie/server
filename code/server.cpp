#include <iostream>
#include <vector>
#include <list>
#include <iterator>
#include <thread>
#include <chrono>
#include "Spider.h"

std::vector<Spider> spiders;
std::vector<std::thread> spider_threads;
#define SPIDER_MAX_NUM 1

int main(int argc, char * const argv[])
{

	spiders.resize(SPIDER_MAX_NUM);
	for (int spider_id = 0; spider_id < SPIDER_MAX_NUM; ++spider_id)
    {
		spider_threads.emplace_back(Spider::initThreadCB, &spiders[spider_id]);
	}
	for(auto& x: spider_threads)
    {
	    x.join();
    }
	return 0;
}
