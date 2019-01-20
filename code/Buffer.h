/*
 * Buffer.h
 *
 *  Created on: Dec 31, 2018
 *      Author: xarvie
 */

#ifndef BUFFER_H_
#define BUFFER_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <netinet/tcp.h>
#include <list>
#include <string>
#include "concurrentqueue.h"

class MessageBuffer
{
public:
	MessageBuffer()
	{
		buff = (char*)malloc(1024);
		capacity = 1024;
	}
	~MessageBuffer()
	{
		free(buff);
	}
	void push_back(int len, const char* buff)
	{
		int newSize = size + len;
		if(newSize > capacity)
		{
			this->capacity = (((size + len)/1024)+2)*1024;
			buff = (char*)realloc(this->buff, this->capacity);
			if(buff == NULL) ;//TODO
		}
		this->size_list.push_back(len);
		memcpy(this->buff + size, buff, len);
		this->size += len;
	}
	void erase(int len)
	{
		for(std::list<int>::iterator it = size_list.begin(); it != size_list.end();)
		{
			if(len >= *it)
			{
				len -= *it;
				it = size_list.erase(it);
				this->size -= *it;
			}
			else
			{
				*it -= len;
				this->size -= len;
				return ;
			}
		}
	}
//private:
	std::list<int> size_list;
	int size = 0;
	char* buff = nullptr;
	int capacity = 0;
};

#endif /* BUFFER_H_ */
