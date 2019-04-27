
#include "SystemReader.h"

#if defined(OS_LINUX) && !defined(SELECT_SERVER)

#include "EpollPoller.h"

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

#ifdef OS_WINDOWS
#define getSockError WSAGetLastError
#define closeSocket closesocket

#else
#define getSockError() errno
#define closeSocket close
#endif

Poller::~Poller() {
    int i = 0, c = 0;

    for (i = 0; i < this->maxWorker; ++i) {
        if (workThreads[i].joinable())
            workThreads[i].join();
    }
    if (listenThread.joinable())
        listenThread.join();

    struct epoll_event evReg;

    for (c = 0; c < CONN_MAXFD; ++c) {
        Session *conn = sessions[c];

        sessions[c]->readBuffer.destroy();
        sessions[c]->writeBuffer.destroy();
    }

    for (int epi = 0; epi < this->maxWorker; ++epi) {
        close(epolls[epi]);
    }
    close(lisSock);
}


extern int send(uint64_t fd, unsigned char *data, int len);

class connector {
    const unsigned MAX_THREAD_NUM = 7;

    void loop();

public:
    std::vector<std::thread> threads;

};

void connectorThread(void *arg) {
    moodycamel::ConcurrentQueue<int> x;

}

void Poller::closeSession(Session* conn) {
    if(conn->readBuffer.size < 0 || conn->writeBuffer.size < 0)
        return;
    int index = conn->sessionId % this->maxWorker;
    linger lingerStruct;

    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    setsockopt(conn->sessionId, SOL_SOCKET, SO_LINGER,
               (char *) &lingerStruct, sizeof(lingerStruct));
    conn->readBuffer.size = -1;
    conn->writeBuffer.size  = -1;
    closeSocket(conn->sessionId);
}

int Poller::sendMsg(uint64_t fd, const Msg &msg) {
    int len = msg.len;
    unsigned char* data = msg.buff;
    Session *conn = this->sessions[fd];
    if(conn->writeBuffer.size > 0)
    {
        conn->writeBuffer.push_back(len, data);
        return 0;
    }
    //free(conn->writeBuffer.buff);
//if (conn->type == 0) { //TODO reconnect continue send?
//        return -1;
//    }

    int ret = write(conn->sessionId, data, len);
    if (ret > 0) {

        this->onWriteBytes(conn->sessionId, len);
        if (ret == len)
            return 0;

        int left = len - ret;
        //conn->writeBuffer.buff = (unsigned char*)malloc(1024);
        //free(conn->writeBuffer.buff);
        conn->writeBuffer.push_back(left, data + ret);

    } else {
        if (errno != EINTR && errno != EAGAIN)
            return -1;

        conn->writeBuffer.push_back(len, data);
    }


    return 0;
}


using namespace std::chrono;
steady_clock::time_point start;
int ixx = 0;
Msg msg;
static int abc = 0;

int Poller::handleReadEvent(Session *conn) {
    if (conn->readBuffer.size < 0)
        return -1;

    unsigned char *buff = conn->readBuffer.buff + conn->readBuffer.size;

    int ret = recv(conn->sessionId, buff, conn->readBuffer.capacity - conn->readBuffer.size, 0);

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
        if (errno != EINTR && errno != EAGAIN) {
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


    int ret = write(conn->sessionId, (void *) conn->writeBuffer.buff,
                    conn->writeBuffer.size);

    if (ret == -1) {

        if (errno != EINTR && errno != EAGAIN) {
            printf("err: write%d\n", errno);
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

void Poller::workerThreadCB(int index) {
    int epfd = this->epolls[index];

    struct epoll_event event[30];
    struct epoll_event evReg;
    bool dequeueRet = false;
    sockInfo event1;
    moodycamel::ConcurrentQueue<sockInfo> &queue = taskQueue[index];
    while (true) {
        while (queue.try_dequeue(event1)) {
            if (event1.event  == ACCEPT_EVENT) {
                int ret = 0;
                int clientFd = event1.fd;

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

                //acceptClientFdsVec.insert((uint64_t) event1.fd);
                sessions[clientFd]->readBuffer.size = 0;
                sessions[clientFd]->writeBuffer.size = 0;

                evReg.data.fd = clientFd;
                evReg.events = EPOLLIN | EPOLLONESHOT;
                this->sessions[clientFd]->sessionId = clientFd;
                epoll_ctl(this->epolls[index], EPOLL_CTL_ADD, clientFd, &evReg);

            }

        }

        int numEvents = epoll_wait(epfd, event, 30, 1000);//TODO wait 1
        sockInfo taskInfo;
        int shutdown = 0;

        if (shutdown) {
            break;
        }

        if (numEvents == -1) {
            //printf("wait\n %d", errno);
        }else if (numEvents > 0) {
            for(int i = 0; i < numEvents; i++)
            {
                int sock = event[i].data.fd;
                Session *conn = this->sessions[sock];
//            if (conn->type == 0) //TODO
//                continue;
                if (event[i].events & EPOLLOUT) {
                    if (this->handleWriteEvent(conn) == -1) {
                        this->closeSession(conn);
                        continue;
                    }
                }

                if (event[i].events & EPOLLIN) {
                    if (this->handleReadEvent(conn) == -1) {
                        this->closeSession(conn);
                        continue;
                    }
                }

                evReg.events = EPOLLIN | EPOLLONESHOT;
                if (conn->writeBuffer.size > 0)
                    evReg.events |= EPOLLOUT;
                evReg.data.fd = sock;
                epoll_ctl(epfd, EPOLL_CTL_MOD, conn->sessionId, &evReg);
            }
        }
    }
}


int Poller::listen(const int port) {
    sockInfo connectSockInfo;
    connectSockInfo.port = port;
    connectSockInfo.task = 1;
    return 0;
}

void Poller::listenThreadCB(int port) {
    int lisEpfd = epoll_create(5);

    struct epoll_event evReg;
    evReg.events = EPOLLIN;
    evReg.data.fd = this->lisSock;


    epoll_ctl(lisEpfd, EPOLL_CTL_ADD, this->lisSock, &evReg);

    struct epoll_event event;

    while (true) {
        int numEvent = epoll_wait(lisEpfd, &event, 1, 1000);

        //TODO con

        std::vector<sockInfo> sockInfoVec;
        int shutdown = 0;
//        while (true) {
//            sockInfo deque;
//            bool success = this->listenTaskQueue.try_dequeue(deque);
//            if (!success)
//                break;
//
//        }
        if (shutdown) {
            break;
        }

        if (numEvent > 0) {

            int sock = accept(this->lisSock, NULL, NULL);
            if (sock > 0) {
//                this->sessions[sock].type = 1; //TODO

                int pollerId = 0;
#ifdef OS_WINDOWS
                pollerId = sock / 4 % maxWorker;
#else
                pollerId = sock % maxWorker;
#endif
                sockInfo x;
                x.fd = sock;
                x.event = ACCEPT_EVENT;
                taskQueue[pollerId].enqueue(x);
            }
        }
    }

    close(lisEpfd);
}

bool Poller::createListenSocket(int port){
    lisSock = socket(AF_INET, SOCK_STREAM, 0);

    int reuse = 1;
    setsockopt(lisSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int nRcvBufferLen = 32 * 1024 * 1024;
    int nSndBufferLen = 32 * 1024 * 1024;
    int nLen = sizeof(int);

    setsockopt(lisSock, SOL_SOCKET, SO_SNDBUF, (char *) &nSndBufferLen, nLen);
    setsockopt(lisSock, SOL_SOCKET, SO_RCVBUF, (char *) &nRcvBufferLen, nLen);

    int flag;
    flag = fcntl(lisSock, F_GETFL);
    fcntl(lisSock, F_SETFL, flag | O_NONBLOCK);

    struct sockaddr_in lisAddr;
    lisAddr.sin_family = AF_INET;
    lisAddr.sin_port = htons(port);
    lisAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(lisSock, (struct sockaddr *) &lisAddr, sizeof(lisAddr)) == -1) {
        printf("bind");
        return false;
    }

    if(::listen(lisSock, 4096) <0){
        printf("listen");
        return false;
    }

    return true;
}
int Poller::run(int port) {
    {/* init */

    }
    {/* init queue  */
        taskQueue.resize(this->maxWorker);
        serverStop = false;
    }
    {/* create pollers*/
        epolls.resize(maxWorker);
        for (int epi = 0; epi < this->maxWorker; ++epi) {
            epolls[epi] = epoll_create(20);
        }
    }
    {/* create listen*/
        this->createListenSocket(port);
    }
    {/* start workers*/
        for (int i = 0; i < this->maxWorker; ++i) {
            workThreads.emplace_back(std::thread([=]{this->workerThreadCB(i);}));
        }
    }
    {/* start listen*/
        listenThread = std::thread([=]{this->listenThreadCB(port);});
    }
    {/* wait exit*/
        listenThread.join();
        for (auto &E: this->workThreads) {
            E.join();
        }
    }
    {/*exit*/
        close(lisSock);
    }
    return 0;
}

void Poller::disconnect(int fd) {
    //sessions[fd].disconnect();
}

int Poller::connect(const char *ip, short port) {
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);

    sockInfo connectSockInfo;
    strcpy(connectSockInfo.ip, ip);
    connectSockInfo.port = port;
    connectSockInfo.fd = socketFd;
    struct sockaddr_in svraddr;
    svraddr.sin_family = AF_INET;
    if (strlen(ip))
        svraddr.sin_addr.s_addr = inet_addr(ip);
    else
        svraddr.sin_addr.s_addr = INADDR_ANY;

    svraddr.sin_port = htons(port);
    int ret = ::connect(socketFd, (struct sockaddr *) &svraddr, sizeof(svraddr));
    if (ret != 0) {
        close(socketFd);
        return false;
    }
    //listenTaskQueue.enqueue(connectSockInfo);
    return 0;
}

#endif