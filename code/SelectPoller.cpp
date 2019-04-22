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
            if (errno != EINTR && errno != EAGAIN)
                return -1;

            leftBytes = len;
            conn->writeBuffer.push_back(len, data);
        }
    }
    //if(leftBytes>0)

    return 0;
}

int Poller::handleReadEvent(Session *conn) {
    unsigned char *buff = conn->readBuffer.buff;// + conn->readBuffer.size;
    int ret = recv(conn->sessionId, (char *)buff, BUFFER_SIZE - 1, 0);

    if (ret > 0) {
        //TODO
        buff[ret] = 0;
        Msg msg;
        msg.len = ret;
        msg.buff = buff;
        onReadMsg(conn->sessionId, msg);

        std::cout << "t:" <<std::this_thread::get_id() << std::endl;

    } else if (ret == 0) {
        return -1;
    } else {
        if (errno != EINTR && errno != EAGAIN  && errno != EWOULDBLOCK) {
            std::cout << "err: recv " << errno << std::endl;
            return -1;
        }
    }
    return 0;
}

int Poller::handleWriteEvent(Session *conn) {
    if (conn->writeBuffer.size == 0)
        return 0;

    int ret = send(conn->sessionId, (char *) conn->writeBuffer.buff,
                    conn->writeBuffer.size, 0);

    if (ret == -1) {

        if (errno != EINTR && errno != EAGAIN) {
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

void Poller::closeConnection(Session *conn) {

}

void Poller::workerThreadCB(Poller *thisPtr, int index) {
    thisPtr->workerThread(index);
}

void Poller::workerThread(int index) {
    moodycamel::ConcurrentQueue<sockInfo> &queue = taskQueue[index];
    sockInfo event;
    std::vector<uint64_t> acceptClientFds;
    acceptClientFds.reserve(1024);
    bool dequeueRet = false;
    std::vector<SOCKET> clients;
    while (true) {
        while (queue.try_dequeue(event)) {
            if (event.event == ACCEPT_EVENT) {
                int ret = 0;
                int clientFd = event.fd;

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
                acceptClientFds.push_back((uint64_t) event.fd);
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
            for (auto &E:acceptClientFds)
                clients.push_back(E);
            acceptClientFds.clear();
            for (int i = 0; i < (int) clients.size(); i++) {
                FD_SET(clients[i], &fdRead);
                FD_SET(clients[i], &fdWrite);
                if (maxSock < clients[i]) {
                    maxSock = clients[i];
                }
            }

            timeval t = {1, 0};
            if(maxSock == 0)
            {
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
                for (int i = 0; i < (int) clients.size(); i++) {
                    if (FD_ISSET(clients[i], &fdRead)) {
                        if (handleReadEvent(sessions[clients[i]]) == -1) {
                            auto iter = clients.begin() + i;
                            if (iter != clients.end()) {
                                close(clients[i]);
                                clients.erase(iter);
                            }
                        }
                    }
                    if (FD_ISSET(clients[i], &fdWrite)) {
                        if (handleWriteEvent(sessions[clients[i]]) == -1) {
                            auto iter = clients.begin() + i;
                            if (iter != clients.end()) {
                                close(clients[i]);
                                clients.erase(iter);
                            }
                        }
                    }
                }
            //do


        }

    }
#ifdef OS_WINDOWS
    for (int i = 0; i < clients.size(); i++) {
        closesocket(clients[i]);
    }
#else
    for (int i = 0; i < clients.size(); i++){
        close(clients[i]);
    }
#endif


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

    this->listen_thread = std::thread(Poller::listenThreadCB, this, port);

    for (int i = 0; i < maxWorker; i++) {
        worker.emplace_back(std::thread(Poller::workerThreadCB, this, i));
    }

    this->listen_thread.join();
    for (auto &E:worker) {
        E.join();
    }


    return 0;
}

Poller::~Poller() {

}

void Poller::listenThreadCB(Poller *thisPtr, int port) {
    thisPtr->listenThread(port);
}

void Poller::listenThread(int port) {

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