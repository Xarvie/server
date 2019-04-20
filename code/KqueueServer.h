
#include "DefConfig.h"
#if defined(OS_DARWIN) && !defined(SELECT_SERVER)

#ifndef SERVER_SPIDER_H_
#define SERVER_ANT_H_
#include "Buffer.h"
#include <vector>
#include <iostream>
#include <string>
#include <list>
#include "queue.h"

class Poller;
struct sockInfo
{
	unsigned short rrindex;
    int port;
    char ip[128];
    int fd;
    char task;/*1:listen 2:connect 3:disconnect*/
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
    enum
    {
        BUFFER_SIZE = 4096
    };

    uint64_t sessionId;
	unsigned short rrindex;
	int type; /*0:null 1:accept 2:connect*/
    int64_t preHeartBeats = 0;

    Poller * spider;
	MessageBuffer writeBuffer;
    MessageBuffer readBuffer;
    const int suggested_capacity = BUFFER_SIZE;
	virtual int cbRead(int readNum);
    virtual int cbAlloc();
    int disconnect();

    struct event_data *client_data = nullptr;

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
    /**
     * API
     */


public:

	virtual ~Poller();


	int sendMsg(uint64_t fd, const Msg &msg);
	int handleReadEvent(Session* conn);
	int handleWriteEvent(Session* conn);
	void closeConnection(Session* conn);
	static void workerThreadCB(Poller* thisPtr, int *epfd, int epindex);
	static void listenThreadCB(Poller* thisPtr, void *arg);
	void workerThread(int *epfd, int epindex);
	void listenThread(void *arg);
	int listen(const int port);
	int connect(const char * ip, const short port);
	int idle();
	int loop(int socketFd, const char * ip, const short port);
	int run(int port);
	static int initThreadCB(Poller* self, int port);
	bool get(Msg& msg);

	void disconnect(int fd);
#define EPOLL_NUM 8
int epfd[EPOLL_NUM];
int lisSock;

std::vector<std::thread> worker;
std::list<std::thread> connectThreads;
std::thread listen_thread;


moodycamel::ConcurrentQueue<sockInfo> listenTaskQueue;
moodycamel::ConcurrentQueue<sockInfo> eventQueue;
std::vector< moodycamel::ConcurrentQueue<sockInfo> > acceptTaskQueue;
moodycamel::ConcurrentQueue<Msg> msgQueue;

#define CONN_MAXFD 65535
    std::vector<Session*> sessions;


public:


	virtual void onAccept(uint64_t sessionId, const Addr &addr) = 0;
	virtual void onReadMsg(uint64_t sessionId, const Msg &msg) = 0;
	virtual void onWriteBytes(uint64_t sessionId, int len) = 0;

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


    int queue;
    unsigned short rrIndex = 0;
    enum {
        ACCEPT_EVENT,
        RW_EVENT
    };
    enum{
        REQ_DISCONNECT,
        REQ_SHUTDOWN,
        REQ_CONNECT
    };
    int _shutdown = 0;
};

#endif /* SERVER_SPIDER_H_ */
#endif
