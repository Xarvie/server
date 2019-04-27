#include "SystemReader.h"

#if defined(SELECT_SERVER)

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include<windows.h>
#include<WinSock2.h>

#pragma comment(lib, "ws2_32.lib")
#else

#define SOCKET int
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)
#endif

#include "SelectPoller.h"

#pragma comment(lib, "ws2_32.lib")

#ifdef OS_WINDOWS
#define getSockError WSAGetLastError
#define closeSocket closesocket

#else
#define getSockError() errno
#define closeSocket close
#endif

int IsEagain()
{
#if defined(OS_WINDOWS)
    int err = getSockError();
    if (err == EINTR || err == EAGAIN  || err == EWOULDBLOCK || err == WSAEWOULDBLOCK) {
        return 1;
    }
    return 0;
#else
    int err = errno;
    if (err == EINTR || err == EAGAIN  || err == EWOULDBLOCK) {
        return 1;
    }
    return 0;
#endif
}
int Poller::sendMsg(uint64_t fd, const Msg &msg) {
    unsigned char *data = msg.buff;
    int len = msg.len;
    Session *conn = this->sessions[fd];
    int leftBytes = 0;
    if (conn->writeBuffer.size > 0) {
        conn->writeBuffer.push_back(len, data);
        return 0;
    } else {
        int ret = send(conn->sessionId, (char*)data, len, 0);
        if (ret > 0) {
            if (ret == len)
                return 0;

            leftBytes = len - ret;
            conn->writeBuffer.push_back(leftBytes, data + ret);
        } else {
            int err = getSockError();

            if (!IsEagain()) {
                std::cout <<"err: send :" << err << std::endl;
                return -1;
            }
            leftBytes = len;
            conn->writeBuffer.push_back(len, data);
        }
    }
    //if(leftBytes>0)

    return 0;
}

int Poller::handleReadEvent(Session *conn) {
    if (conn->readBuffer.size < 0)
        return -1;

    unsigned char *buff = conn->readBuffer.buff + conn->readBuffer.size;

    int ret = recv(conn->sessionId, (char*)buff, conn->readBuffer.capacity - conn->readBuffer.size, 0);

    if (ret > 0) {
        conn->readBuffer.size += ret;
        conn->readBuffer.alloc();
        if(conn->readBuffer.size > 1024 * 1024 * 3)
        {
            return -1;
            //TODO close socket
        }
        //TODO
        int readBytes = onReadMsg(conn->sessionId, ret);
        conn->readBuffer.size -= readBytes;
        if(conn->readBuffer.size < 0)
            return -1;
    } else if (ret == 0) {
        return -1;
    } else {
        int err = getSockError();

        if (!IsEagain()) {
            std::cout << "err: recv " << err << std::endl;
            return -1;
        }
    }
    return 0;
}

int Poller::handleWriteEvent(Session *conn) {
    if (conn->writeBuffer.size == 0)
        return 0;
    if (conn->writeBuffer.size < 0)
        return -1;

    int ret = send(conn->sessionId, (char *) conn->writeBuffer.buff,
                    conn->writeBuffer.size, 0);

    if (ret == -1) {
        int err = getSockError();

        if (!IsEagain()) {
            printf("err:write %d\n", errno);
            return -1;
        }
    } else if (ret == 0) {
        //TODO
    } else {
        onWriteBytes(conn->sessionId, ret);
        conn->writeBuffer.erase(ret);
    }
    return 0;
}

void Poller::closeSession(Session *conn) {
    if(conn->readBuffer.size < 0 || conn->writeBuffer.size < 0)
        return;
    int index = conn->sessionId/4 % this->maxWorker;
    std::set<uint64_t >& clientVec = this->clients[index];
    std::set<uint64_t>& acceptClientFdsVec = this->acceptClientFds[index];
    acceptClientFdsVec.erase(conn->sessionId);


    linger lingerStruct;

    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    setsockopt(conn->sessionId, SOL_SOCKET, SO_LINGER,
               (char *) &lingerStruct, sizeof(lingerStruct));
    conn->readBuffer.size = -1;
    conn->writeBuffer.size  = -1;
    closeSocket(conn->sessionId);
}

void Poller::workerThreadCB(int index) {
    std::set<uint64_t>& clientVec = this->clients[index];
    std::set<uint64_t>& acceptClientFdsVec = this->acceptClientFds[index];
    moodycamel::ConcurrentQueue<sockInfo> &queue = taskQueue[index];
    sockInfo event;

    bool dequeueRet = false;

    while (true) {
        while (queue.try_dequeue(event)) {
            if (event.event == ACCEPT_EVENT) {
                int ret = 0;
                int clientFd = event.fd;

                int nRcvBufferLen = 80 * 1024;
                int nSndBufferLen = 1 * 1024 * 1024;
                int nLen = sizeof(int);

                setsockopt(clientFd, SOL_SOCKET, SO_SNDBUF, (char *) &nSndBufferLen, nLen);
                setsockopt(clientFd, SOL_SOCKET, SO_RCVBUF, (char *) &nRcvBufferLen, nLen);
                int nodelay = 1;
                if (setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                               sizeof(nodelay)) < 0)
                    perror("error: nodelay");
#if defined(OS_WINDOWS)
                unsigned long ul = 1;
                ret = ioctlsocket(clientFd, FIONBIO, (unsigned long *) &ul);//设置成非阻塞模式。
                if (ret == SOCKET_ERROR)
                    printf("Could not get server socket flags: %s\n", strerror(errno));
#else
                int flags = fcntl(clientFd, F_GETFL, 0);
                if (flags < 0) printf("Could not get server socket flags: %s\n", strerror(errno));

                ret = fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
                if (ret < 0) printf("Could set server socket to be non blocking: %s\n", strerror(errno));
#endif

                acceptClientFdsVec.insert((uint64_t) event.fd);
                sessions[clientFd]->readBuffer.size = 0;
                sessions[clientFd]->writeBuffer.size = 0;
            }

        }


        {

            fd_set fdRead;//描述符（socket） 集合
            fd_set fdWrite;
            fd_set fdExp;
            //清理集合
            FD_ZERO(&fdRead);
            FD_ZERO(&fdWrite);
            FD_ZERO(&fdExp);

            SOCKET maxSock = 0;//TODO
            for (auto &E:acceptClientFdsVec)
                clientVec.insert(E);
            acceptClientFdsVec.clear();
            for (auto& E :clientVec) {
                FD_SET(E, &fdRead);
                FD_SET(E, &fdWrite);
                if (maxSock < E) {
                    maxSock = E;
                }
            }
            int sec = 1;
            timeval t = {sec, 0};
            if(maxSock == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(sec*1000));
                continue;
            }

            int ret = select(maxSock + 1, &fdRead, &fdWrite, &fdExp, &t);

            if (ret < 0) {

#if !defined(OS_WINDOWS)
                std::cout << "err: select" << errno << std::endl;
#else
                    std::cout << "err: select " << WSAGetLastError() << std::endl;
#endif
                break;
            }

            if (ret > 0)
            {
                for(auto iter = clientVec.begin(); iter != clientVec.end(); )
                {
                    uint64_t fd = *iter;
                    bool needDel = false;
                    if (FD_ISSET(fd, &fdRead)) {
                        if (handleReadEvent(sessions[fd]) == -1) {
                            this->closeSession(sessions[fd]);
                            needDel = 1;
                            iter = clientVec.erase(iter);
                            continue;
                        }
                    }
                    if (FD_ISSET(fd, &fdWrite)) {
                        if (handleWriteEvent(sessions[fd]) == -1) {
                            this->closeSession(sessions[fd]);
                            needDel = 1;
                            iter = clientVec.erase(iter);
                            continue;
                        }
                    }
                    iter++;
                }
            }
                for (int i = 0; i < (int) clientVec.size(); i++) {

                }


        }

    }

    for(auto iter = clientVec.begin(); iter != clientVec.end(); iter++)
    {
        this->closeSession(sessions[*iter]);
    }

}

int Poller::run(int port) {
#ifdef OS_WINDOWS
    WORD ver = MAKEWORD(2, 2);
    WSADATA dat;
    WSAStartup(ver, &dat);

#else
    signal(SIGPIPE, SIG_IGN);
#endif
    taskQueue.resize(maxWorker);
    clients.resize(maxWorker);
    acceptClientFds.resize(maxWorker);

    {/* start workers*/
        for (int i = 0; i < this->maxWorker; ++i) {
            workThreads.emplace_back(std::thread([=] { this->workerThreadCB(i); }));
        }
    }
    {/* start listen*/
        this->listenThread = std::thread([=] { this->listenThreadCB(port); });
    }
    this->listenThread.join();
    for (auto &E:workThreads) {
        E.join();
    }


    return 0;
}

Poller::~Poller() {

}

void Poller::listenThreadCB(int port) {

    this->lisSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in _sin = {};
    _sin.sin_family = AF_INET;
    _sin.sin_port = htons(port);
#ifdef OS_WINDOWS
    _sin.sin_addr.S_un.S_addr = INADDR_ANY;
#else
    _sin.sin_addr.s_addr = INADDR_ANY;
#endif

#ifdef OS_WINDOWS
    //TODO
#else
    int opt_val = 1;
    setsockopt(this->lisSock, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
#endif

    if (SOCKET_ERROR == bind(this->lisSock, (sockaddr *) &_sin, sizeof(_sin)))
        printf("err: bind\n");

    if (SOCKET_ERROR == ::listen(this->lisSock, 1024))
        printf("err: listen\n");

    sockaddr_in clientAddr = {};
    int nAddrLen = sizeof(sockaddr_in);
    SOCKET _cSock = INVALID_SOCKET;
    while (true) {
#ifdef OS_WINDOWS
        _cSock = accept(this->lisSock, (sockaddr *) &clientAddr, &nAddrLen);
#else
        _cSock = accept(this->lisSock, (sockaddr *) &clientAddr, (socklen_t *) &nAddrLen);
#endif
        if (INVALID_SOCKET == _cSock) {
            printf("err: accept\n");
            exit(-2);
        }
        int pollerId = 0;
#ifdef OS_WINDOWS
        pollerId = _cSock / 4 % maxWorker;
#else
        pollerId = _cSock % maxWorker;
#endif
        sockInfo x;
        x.fd = _cSock;
        x.event = ACCEPT_EVENT;
        taskQueue[pollerId].enqueue(x);
    }
}


#endif