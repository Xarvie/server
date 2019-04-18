#ifndef BUFFER_H_
#define BUFFER_H_
//#include "DefConfig.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
//#include <netinet/tcp.h>
#include <list>
#include <string>
#include <iostream>

//#include "concurrentqueue.h"
//#include "block.h"

enum
{
    BUFFER_SIZE = 4096,
    HEARD_SIZE = 4
};
class MessageBuffer
{
public:

    void reset()
    {
//        if(buff != nullptr)
//            destroy();
        buff = (unsigned char*)malloc(BUFFER_SIZE);
        capacity = BUFFER_SIZE;
        size = 0;
    }
    void destroy()
    {
        capacity = 0;
        free(buff);
        buff = nullptr;
        size = 0;
    }

    void push_back(int len, const unsigned char* buff1)
    {
        int newSize = size + len;
        if(newSize > capacity)
        {
            this->capacity = (((size + len)/BUFFER_SIZE)+3)*BUFFER_SIZE;
            this->buff = (unsigned char*)::realloc(this->buff, this->capacity);
            if(buff == nullptr)
                ;//TODO
        }

        memcpy(this->buff + size, buff1, len);
        this->size += len;
    }

    void record(int size)
    {
        this->size += size;
    }
    inline void erase(int len)
    {
        memmove(this->buff, this->buff+len, this->size - len);
        this->size -= len;
    }
//private:
    std::list<int> size_list;
    int size = 0;
    unsigned char* buff = nullptr;
    unsigned char* buff2 = nullptr;
    int capacity = 0;
};

#endif /* BUFFER_H_ */
