
#include "SystemReader.h"

#if defined(SELECT_SERVER)
#ifndef SERVER_SELECTSERVER_H
#define SERVER_SELECTSERVER_H

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
    void workerThreadCB(int epindex);
    void listenThreadCB(int port);
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
    std::vector<Session*> sessions;
    int maxWorker = 4;
    std::vector<int> epolls;
    uint64_t lisSock;
    std::vector<std::thread> workThreads;
    std::thread listenThread;
    moodycamel::ConcurrentQueue<sockInfo> eventQueue;
    std::vector< moodycamel::ConcurrentQueue<sockInfo> > taskQueue;

};

#endif //SERVER_SELECTSERVER_H
#endif