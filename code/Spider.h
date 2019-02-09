/*
 * Spider.h
 *
 *  Created on: Jan 19, 2019
 *      Author: xarvie
 */

#ifndef SERVER_SPIDER_H_
#define SERVER_SPIDER_H_
#include "Buffer.h"
#include <vector>
#include <iostream>
#include <string>
#include <list>

class Spider;
struct sockInfo
{
	//TODO move construct
	unsigned short rrindex;
    int port;
    char ip[128];
    int fd;
    int ret;
    char task;/*1:listen 2:connect 3:disconnect*/
    char event;/*1 listen 2:connect*/
};

class Msg
{
public:
	Msg()
	{
		fd = 0;
	}
	Msg(const Msg& r)
	{
		this->buffer = r.buffer;
		this->fd = r.fd;
	}

	Msg(Msg &&a)
	{
		//std::cout << "a" << std::endl;
	}

	Msg& operator = (Msg &&rhs) noexcept
	{
		//std::cout << "b" << std::endl;
		this->buffer = rhs.buffer;
		this->fd = rhs.fd;
		return *this;
	}

    int fd;
    MessageBuffer buffer;
};

class connection
{
	//TODO move construct
public:
	enum
	{
		BUFFER_SIZE = 4096
	};
	int sock;
	unsigned short rrindex;
	int type; /*0:null 1:accept 2:connect*/

    Spider * spider;
	MessageBuffer writeBuffer;
	MessageBuffer readBuffer;
	const int suggested_capacity = BUFFER_SIZE;
	virtual int cbRead(int readNum);
	virtual int cbAlloc();
	int disconnect();

};

class Spider
{
public:
	Spider(int port);
	virtual ~Spider();

    Spider(Spider &&a)
    {
        std::cout << "a" << std::endl;
    }

    Spider& operator = (Spider &&rhs) noexcept
    {
        std::cout << "a" << std::endl;
        return *this;
    }

	int send(int fd, char *data, int len);
	int handleReadEvent(connection* conn);
	int handleWriteEvent(connection* conn);
	void closeConnection(connection* conn);
	static void workerThreadCB(Spider* thisPtr, int *epfd, int epindex);
	static void listenThreadCB(Spider* thisPtr, void *arg);
	void workerThread(int *epfd, int epindex);
	void listenThread(void *arg);
	int listen(int port);
	int connect(const char * ip, short port);
	int idle();
	int loop(int socketFd, const char * ip, const short port);
	int init(int port);
	static int initThreadCB(Spider* self, int port);
    void disconnect(int fd);
    bool get(Msg& msg);
enum
{
	BUFFER_SIZE = 4096
};
	enum {
		ACCEPT_EVENT,
		RW_EVENT
	};
	enum{
		REQ_DISCONNECT,
		REQ_SHUTDOWN,
		REQ_CONNECT
	};
#define CONN_MAXFD 65536
std::vector<connection> m_conn_table;
#define EPOLL_NUM 8


    int epfd[EPOLL_NUM];
    int lisSock;
    std::vector<std::thread> worker;
    std::list<std::thread> connectThreads;
    std::thread listen_thread;
    std::thread init_thread;
    moodycamel::ConcurrentQueue<sockInfo> listenTaskQueue;
    moodycamel::ConcurrentQueue<sockInfo> eventQueue;
    std::vector< moodycamel::ConcurrentQueue<sockInfo> > acceptTaskQueue;
    moodycamel::ConcurrentQueue<Msg> msgQueue;
};

#endif /* SERVER_SPIDER_H_ */
