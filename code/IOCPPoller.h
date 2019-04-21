#include "SystemReader.h"
#if defined(OS_WINDOWS) && !defined(SELECT_SERVER)

#ifndef IOCPSERVER_H
#define IOCPSERVER_H

#include <mswsock.h>
#include <vector>
#include "queue.h"
#include "NetStruct.h"

#define MAX_BUFF_SIZE       8192

#define xmalloc malloc
#define xfree free
#include "Buffer.h"

enum RWMOD {
    ClientIoAccept,
    ClientIoConnect,
    ClientIoRead,
    ClientIoWrite
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

//关闭时需要close全部client
struct PER_SOCKET_CONTEXT {
    WSAOVERLAPPED Overlapped;
    char Buffer[MAX_BUFF_SIZE];
    WSABUF wsabuf;
    int nTotalBytes;
    int nSentBytes;
    RWMOD IOOperation;
    SOCKET SocketAccept;
    uint64_t sessionId;
};

class Session {
public:
    uint64_t sessionId;
    //void* ptr = NULL;
    int64_t preHeartBeats = 0;
    MessageBuffer readBuffer;
    MessageBuffer writeBuffer;
    PER_SOCKET_CONTEXT* iocp_context = NULL;
    PER_SOCKET_CONTEXT* iocp_write_context = NULL;
    /*
     *
     * 游戏逻辑调用发送，单线程内直接发送一次，发送成功一部分的话，剩余部分写入这个buffer里，
     * 等待poller响应到（linux可write）(windows完成write)时判断size>0就继续发送。
     *
     * */

    void reset()
    {
        sessionId = 0;
        preHeartBeats = 0;
        readBuffer.reset();
        writeBuffer.reset();
        if(iocp_context == NULL)
            iocp_context = (PER_SOCKET_CONTEXT*)xmalloc(sizeof(PER_SOCKET_CONTEXT));
        memset(iocp_context, 0, sizeof(PER_SOCKET_CONTEXT));

        if(iocp_write_context == NULL)
            iocp_write_context = (PER_SOCKET_CONTEXT*)xmalloc(sizeof(PER_SOCKET_CONTEXT));
        memset(iocp_write_context, 0, sizeof(PER_SOCKET_CONTEXT));

        //iocp_context = NULL;
    }
};


class Poller {
public:
    virtual void onAccept(uint64_t sessionId, const Addr &addr) = 0;
    virtual void onReadMsg(uint64_t sessionId, const Msg &msg) = 0;
    virtual void onWriteBytes(uint64_t sessionId, int len) = 0;
    int continueSendMsg(uint64_t sessionId);
    void sendMsg(uint64_t sessionId, const Msg &msg);
    BOOL CreateListenSocket(void);
    int connect(std::string ip, std::string port);

    static DWORD WorkerThread(Poller *self, LPVOID WorkContext);

    PER_SOCKET_CONTEXT* UpdateCompletionPort(int workerId, SOCKET s, RWMOD ClientIo, BOOL bAddToList);
    int closeSession(uint64_t sessionId);

    VOID CloseClient(PER_SOCKET_CONTEXT* lpPerSocketContext, BOOL bGraceful);

    PER_SOCKET_CONTEXT* CtxtAllocate(SOCKET s, RWMOD ClientIO);

    int run(int port);

    const char *g_Port = "9876";
    BOOL g_bEndServer = FALSE;
    BOOL g_bVerbose = FALSE;
    std::vector<HANDLE> iocps;
    SOCKET g_sdListen = INVALID_SOCKET;
    int maxWorker = 4;
    std::thread listenThread;
    std::vector<std::list<Session*>*> onlineSessionLists;
    std::vector<std::thread> workThreads;
    std::vector<Session*> sessions;

    std::vector<moodycamel::ConcurrentQueue<int>> taskQueue;

};

#endif
#endif