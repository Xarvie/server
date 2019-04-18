#include "config.h"
#ifdef OS_LINUX

#ifndef SERVER_SPIDER_H_
#define SERVER_SPIDER_H_
#include "Buffer.h"
#include <vector>
#include <iostream>
#include <string>
#include <list>
#include "queue.h"

#define MAX_BUFF_SIZE       8192

#define xmalloc malloc
#define xfree free

class Poller;
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

struct Addr {
	std::string ip;
	std::string port;
};

struct Msg {
	int len;
	unsigned char *buff;
};

union RawSocket {
	int unixSocket;
	void *windowsSocket = nullptr;
};

class Session
{
public:
	//TODO move construct
	uint64_t sessionId;
	unsigned short rrindex;
	int64_t preHeartBeats = 0;
	MessageBuffer writeBuffer;
	MessageBuffer readBuffer;

	void reset()
	{
		sessionId = 0;
		rrindex = 0;
		preHeartBeats = 0;
		readBuffer.reset();
		writeBuffer.reset();
	}
};

class Poller
{
public:

	virtual ~Poller();
	/*
	Poller(int port);
    Poller(Poller &&a);

    Poller& operator = (Poller &&rhs) noexcept;
	*/
	virtual void onAccept(uint64_t sessionId, const Addr &addr) = 0;
	virtual void onReadMsg(uint64_t sessionId, const Msg &msg) = 0;
	virtual void onWriteBytes(uint64_t sessionId, int len) = 0;


	int sendMsg(uint64_t fd, const Msg &msg);
	int handleReadEvent(Session* conn);
	int handleWriteEvent(Session* conn);
	void closeConnection(Session* conn);
	static void workerThreadCB(Poller* thisPtr, std::vector<int>* epfd, int epindex);
	static void listenThreadCB(Poller* thisPtr, void *arg);
	void workerThread(std::vector<int>* epfd, int epindex);
	void listenThread(void *arg);
	int listen(int port);
	int connect(const char * ip, short port);
	int run(int port);
    void disconnect(int fd);

	enum {
		ACCEPT_EVENT,
		RW_EVENT
	};
	enum{
		REQ_DISCONNECT,
		REQ_SHUTDOWN,
		REQ_CONNECT
	};
#define CONN_MAXFD 65535
Session* sessions[65535] ;
#define EPOLL_NUM 8

	int maxWorker = 4;
	std::vector<int> epolls;
    int lisSock;
    std::vector<std::thread> worker;
    std::thread listen_thread;
    std::thread init_thread;
    moodycamel::ConcurrentQueue<sockInfo> listenTaskQueue;
    moodycamel::ConcurrentQueue<sockInfo> eventQueue;
    std::vector< moodycamel::ConcurrentQueue<sockInfo> > acceptTaskQueue;
};

#endif /* SERVER_SPIDER_H_ */
#endif