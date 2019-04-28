#include "SystemReader.h"

#if defined(OS_DARWIN) && !defined(SELECT_SERVER)

#ifndef KQUEUEPOLLER_H
#define KQUEUEPOLLER_H

#include "NetStruct.h"
#include "Buffer.h"

class Session {
public:

    uint64_t sessionId;
    int type; /*0:null 1:accept 2:connect*/
    int64_t preHeartBeats = 0;

    MessageBuffer writeBuffer;
    MessageBuffer readBuffer;
    const int suggested_capacity = BUFFER_SIZE;

    virtual int cbRead(int readNum);

    virtual int cbAlloc();

    int disconnect();

    struct event_data *client_data = nullptr;

    void reset() {
        sessionId = 0;
        preHeartBeats = 0;
        readBuffer.reset();
        writeBuffer.reset();
    }
};


class Poller {
public:
    Poller(int port, int threadsNum);
    virtual ~Poller();


    int sendMsg(uint64_t fd, const Msg &msg);

    void closeSession(Session *conn);

    void workerThreadCB(int pollerIndex);

    void listenThreadCB(int port);

    int connect(const char *ip, const short port);

    int run(int port);

    void disconnect(int fd);

    int on_read(struct kevent *event);

    int on_write(struct kevent *event);


    int lisSock;

    int maxWorker = 0;

    std::vector<int> queue;
    struct kevent *events;
    std::vector<struct kevent> event_set;
    std::vector<struct kevent*> event_list;
    int events_used = 0;
    int events_alloc = 0;
    volatile bool isRunning = false;
    std::vector<std::thread> workerThreads;
    std::list<std::thread> connectThreads;
    std::thread listenThread;


    moodycamel::ConcurrentQueue<sockInfo> listenTaskQueue;
    moodycamel::ConcurrentQueue<sockInfo> eventQueue;
    std::vector<moodycamel::ConcurrentQueue<sockInfo> > acceptTaskQueue;

    std::vector< moodycamel::ConcurrentQueue<sockInfo> > taskQueue;
    moodycamel::ConcurrentQueue<Msg> msgQueue;
    std::vector<Session *> sessions;


public:


    virtual int onAccept(Session &conn const Addr &addr) = 0;

    virtual int onReadMsg(Session &conn, int bytesNum) = 0;

    virtual int onWriteBytes(Session &conn int len) = 0;

    void event_change(int ident, int filter, int flags, void *udata);


    enum {
        ACCEPT_EVENT,
        RW_EVENT
    };
    enum {
        REQ_DISCONNECT,
        REQ_SHUTDOWN,
        REQ_CONNECT
    };
    int _shutdown = 0;
};

#endif /* KQUEUEPOLLER_H */
#endif
