/*
 * Spider.h
 *
 *  Created on: Jan 19, 2019
 *      Author: xarvie
 */
#include "DefConfig.h"
#ifdef OS_DARWIN
#ifndef SERVER_SPIDER_H_
#define SERVER_ANT_H_
#include "Buffer.h"
#include <vector>
#include <iostream>
#include <string>
#include <list>

class Spider;
struct sockInfo
{
	unsigned short rrindex;
    int port;
    std::string ip;
    int fd;
    int ret;
    int task;/*1:listen 2:connect 3:disconnect*/
    int event;/*1 listen 2:connect*/
};

class connection
{
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

#if defined(OS_DARWIN)
    struct event_data *client_data = nullptr;
#endif
};


class Spider
{
public:
    /**
     * API
     */


public:
	Spider();
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

	int sendData(connection* conn, char *data, int len);
	int handleReadEvent(connection* conn);
	int handleWriteEvent(connection* conn);
	void closeConnection(connection* conn);
	static void workerThreadCB(Spider* thisPtr, int *epfd, int epindex);
	static void listenThreadCB(Spider* thisPtr, void *arg);
	void workerThread(int *epfd, int epindex);
	void listenThread(void *arg);
	int listen(const int port);
	int connect(const char * ip, const short port);
	int idle();
	int loop(int socketFd, const char * ip, const short port);
	int start(int port);
	static int initThreadCB(Spider* self, int port);


#define CONN_MAXFD 65536
connection m_conn_table[CONN_MAXFD];

sig_atomic_t shut_server = 0;


#define EPOLL_NUM 8
int epfd[EPOLL_NUM];
int lisSock;

std::vector<std::thread> worker;
std::list<std::thread> connectThreads;
std::thread listen_thread;

moodycamel::ConcurrentQueue<sockInfo> listenTaskQueue;
moodycamel::ConcurrentQueue<sockInfo> eventQueue;
std::vector< moodycamel::ConcurrentQueue<sockInfo> > acceptTaskQueue;



public:
    void event_server_listen (int port);
    void event_change(int ident, int filter, int flags, void *udata);
    void event_loop();
    int event_flush_write (struct event_data *self, struct kevent *event);
    int event_on_read(struct event_data *self, struct kevent *event);
    int event_on_write (struct event_data *self, struct kevent *event);
    int event_on_accept (struct event_data *self, struct kevent *event);

    struct kevent *events;
    int events_used = 0;
    int events_alloc = 0;

    struct sockaddr_in server;
    int server_fd, queue;
    unsigned short rrIndex = 0;
    enum {
        ACCEPT_EVENT,
        RW_EVENT
    };
    enum{
        REQ_DISCONNECT,
        REQ_SHUTDOWN
    };
    int _shutdown = 0;
};

#endif /* SERVER_SPIDER_H_ */
#endif
