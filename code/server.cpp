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
std::vector<Spider> spiders;
#define SPIDER_MAX_NUM 1
int main(int argc, char * const argv[])
{
	spiders.resize(SPIDER_MAX_NUM);
	return 0;
}
