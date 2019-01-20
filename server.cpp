#include <iostream>
#include <vector>
#include <list>
#include <iterator>
#include <thread>
#include <chrono>
#include "Spider.h"
std::vector<Spider> spiders;
std::vector<std::thread> spider_threads;
#define SPIDER_MAX_NUM 2
int main1(int argc, char * const argv[])
{
	for (int spider_id = 0; spider_id < SPIDER_MAX_NUM; ++spider_id)
	{
		//spider_threads.emplace_back(Spider::initThreadCB, spiders[spider_id]);
	}
	return 0;
}

#include <iostream>
#include <utility>
#include <vector>
#include <string>

int main0()
{
    std::string str = "Hello";
    std::vector<std::string> v;

    // uses the push_back(const T&) overload, which means
    // we'll incur the cost of copying str
    v.push_back(str);
    std::cout << "After copy, str is \"" << str << "\"\n";

    // uses the rvalue reference push_back(T&&) overload,
    // which means no strings will be copied; instead, the contents
    // of str will be moved into the vector.  This is less
    // expensive, but also means str might now be empty.
    //v.push_back(std::move(str));
    std::cout << "After move, str is \"" << str << "\"\n";

    std::cout << "The contents of the vector are \"" << v[0]
                                         << "\", \"" << v[1] << "\"\n";
}

#include <iostream>
#include <utility>
#include <vector>
#include <string>

int main()
{
    std::string str = "Hello";
    std::vector<std::string> v;

    // uses the push_back(const T&) overload, which means
    // we'll incur the cost of copying str
    v.push_back(str);
    std::cout << "After copy, str is \"" << str << "\"\n";

    // uses the rvalue reference push_back(T&&) overload,
    // which means no strings will be copied; instead, the contents
    // of str will be moved into the vector.  This is less
    // expensive, but also means str might now be empty.
    v.push_back(std::move(str));
    std::cout << "After move, str is \"" << str << "\"\n";

    std::cout << "The contents of the vector are \"" << v[0]
                                         << "\", \"" << v[1] << "\"\n";
}
