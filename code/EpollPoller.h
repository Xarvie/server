#include "SystemReader.h"
#if defined(OS_LINUX) && !defined(SELECT_SERVER)

#ifndef EPOLLPOLL_H_
#define EPOLLPOLL_H_

#include "NetStruct.h"

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
	virtual void onAccept(uint64_t sessionId, const Addr &addr) = 0;
	virtual void onReadMsg(uint64_t sessionId, const Msg &msg) = 0;
	virtual void onWriteBytes(uint64_t sessionId, int len) = 0;


	int sendMsg(uint64_t fd, const Msg &msg);
	int handleReadEvent(Session* conn);
	int handleWriteEvent(Session* conn);
	void closeConnection(Session* conn);
	static void workerThreadCB(Poller* thisPtr, int epindex);
	static void listenThreadCB(Poller* thisPtr, int port);
	void workerThread(int epindex);
	void listenThread(int port);
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
#define CONN_MAXFD 65535
std::vector<Session*> sessions;

	int maxWorker = 4;
	std::vector<int> epolls;
    int lisSock;
    std::vector<std::thread> workThreads;
    std::thread listen_thread;
    std::thread init_thread;
	std::vector< moodycamel::ConcurrentQueue<sockInfo> > taskQueue;
	bool serverStop = 0;
};

#endif /* SERVER_EPOLLPOLL_H_ */
#endif