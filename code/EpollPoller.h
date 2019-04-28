#include "SystemReader.h"
#if defined(OS_LINUX) && !defined(SELECT_SERVER)

#ifndef EPOLLPOLL_H_
#define EPOLLPOLL_H_

#include "NetStruct.h"
#include "Buffer.h"

class Session
{
public:
	uint64_t sessionId;
	MessageBuffer writeBuffer;
	MessageBuffer readBuffer;

	void reset()
	{
		sessionId = 0;
		readBuffer.reset();
		writeBuffer.reset();
	}
};

class Poller
{
public:
    Poller(int port, int threadsNum);
	virtual ~Poller();
	virtual int onAccept(Session &conn, const Addr &addr) = 0;
	virtual int onReadMsg(Session &conn, int byteNum) = 0;
	virtual int onWriteBytes(Session &conn, int len) = 0;


	int sendMsg(uint64_t fd, const Msg &msg);
	int handleReadEvent(Session* conn);
	int handleWriteEvent(Session* conn);

	void closeSession(Session* fd);

	void workerThreadCB(int index);
	void listenThreadCB(int port);
	int listen(int port);
	int connect(const char * ip, short port);
	bool createListenSocket(int port);
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

    std::vector<Session*> sessions;

	int maxWorker = 4;
	std::vector<int> epolls;
    int lisSock;
    std::vector<std::thread> workThreads;
    std::thread listenThread;
	std::vector< moodycamel::ConcurrentQueue<sockInfo> > taskQueue;
	volatile bool isRunning = false;
};

#endif /* SERVER_EPOLLPOLL_H_ */
#endif