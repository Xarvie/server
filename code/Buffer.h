/*
 * Buffer.h
 *
 *  Created on: Dec 31, 2018
 *      Author: xarvie
 */

#ifndef BUFFER_H_
#define BUFFER_H_
#include "DefConfig.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
#include <iostream>

#include "concurrentqueue.h"

class MessageBuffer
{
public:
	MessageBuffer()
	{
		buff = (char*)malloc(1024);
		std::cout << "alloc malloc" << std::endl;
		capacity = 1024;
	}
	~MessageBuffer()
	{
		capacity = 0;
		if(buff != NULL)
			free(buff);
		buff = NULL;
	}
	MessageBuffer(const MessageBuffer& r)
	{
		this->size_list = r.size_list;
		this->size = r.size;
		this->buff = (char*)malloc((size_t)r.capacity);
		std::cout << "copy construct malloc" << std::endl;
		memcpy(this->buff, r.buff, r.size);
		this->capacity = r.capacity;
	}

	MessageBuffer& operator=(const MessageBuffer& r)
	{
		this->size_list = r.size_list;
		this->size = r.size;
		this->buff = (char*)malloc((size_t)r.capacity);
		std::cout << "op= malloc" << std::endl;
		memcpy(this->buff, r.buff, r.size);
		this->capacity = r.capacity;
		return *this;
	}



	void push_back(int len, const char* buff)
	{
		int newSize = size + len;
		if(newSize > capacity)
		{
			this->capacity = (((size + len)/1024)+2)*1024;
			buff = (char*)realloc(this->buff, this->capacity);
			if(buff == NULL)
				;//TODO
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
				this->size -= *it;;
				len -= *it;
				it = size_list.erase(it);
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
