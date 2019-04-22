
#include "SystemReader.h"

#if defined(OS_LINUX) && !defined(SELECT_SERVER)

#include "EpollPoller.h"


Poller::~Poller() {
    init_thread.join();
    int i = 0, c = 0;

    for (i = 0; i < this->maxWorker; ++i) {
        if (worker[i].joinable())
            worker[i].join();
    }
    if (listen_thread.joinable())
        listen_thread.join();

    struct epoll_event evReg;

    for (c = 0; c < CONN_MAXFD; ++c) {
        Session *conn = sessions[c];
//        if (conn->type) {//TODO
//            epoll_ctl(epolls[conn->rrindex % EPOLL_NUM], EPOLL_CTL_DEL, conn->sessionId, &evReg);
//            close(conn->sessionId);
//        }
        sessions[c]->readBuffer.destroy();
        sessions[c]->writeBuffer.destroy();
    }

    for (int epi = 0; epi < this->maxWorker; ++epi) {
        close(epolls[epi]);
    }
    close(lisSock);
}

class Session;

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

int Poller::sendMsg(uint64_t fd, const Msg &msg) {
    int len = msg.len;
    unsigned char* data = msg.buff;
    Session *conn = this->sessions[fd];
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
    unsigned char *buff = conn->readBuffer.buff;// + conn->readBuffer.size;

    int ret = read(conn->sessionId, buff, BUFFER_SIZE - 1);

    if (ret > 0) {
        //TODO
        buff[ret] = 0;
        Msg msg;
        msg.len = ret;
        msg.buff = buff;
        onReadMsg(conn->sessionId, msg);
        //std::cout << buff << std::endl;
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

#if defined(OS_LINUX)

void Poller::closeConnection(Session *conn) {

    struct epoll_event evReg;
    close(conn->sessionId);
    conn->readBuffer.erase(conn->readBuffer.size);
    conn->writeBuffer.erase(conn->writeBuffer.size);
    epoll_ctl(epolls[conn->rrindex % this->maxWorker], EPOLL_CTL_DEL, conn->sessionId, &evReg);
}

#endif

void Poller::workerThreadCB(Poller *thisPtr/*TODO bug?*/, int epindex) {
    thisPtr->workerThread(epindex);
}

void Poller::workerThread(int epindex) {
    int epfd = this->epolls[epindex];

    struct epoll_event event;
    struct epoll_event evReg;

    while (true) {
        int numEvents = epoll_wait(epfd, &event, 1, 1000);//TODO wait 1
        sockInfo taskInfo;
        int shutdown = 0;

        if (shutdown) {
            break;
        }

        if (numEvents == -1) {
            //printf("wait\n %d", errno);
        }

        if (numEvents > 0) {
            int sock = event.data.fd;
            Session *conn = this->sessions[sock];
//            if (conn->type == 0) //TODO
//                continue;
            if (event.events & EPOLLOUT) {
                if (this->handleWriteEvent(conn) == -1) {
                    this->closeConnection(conn);
                    continue;
                }
            }

            if (event.events & EPOLLIN) {
                if (this->handleReadEvent(conn) == -1) {
                    this->closeConnection(conn);
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

void Poller::listenThreadCB(Poller *thisPtr/*TODO bug?*/, int port) {
    thisPtr->listenThread(port);
}

int Poller::listen(const int port) {
    sockInfo connectSockInfo;
    connectSockInfo.port = port;
    connectSockInfo.task = 1;
    return 0;
}

void Poller::listenThread(int port) {
    int lisEpfd = epoll_create(5);

    struct epoll_event evReg;
    evReg.events = EPOLLIN;
    evReg.data.fd = this->lisSock;


    epoll_ctl(lisEpfd, EPOLL_CTL_ADD, this->lisSock, &evReg);

    struct epoll_event event;

    unsigned short rrIndex = 0; /* round robin rrindex */

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

                int nodelay = 1;
                if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                               sizeof(nodelay)) < 0)
                    perror("error: nodelay");

                int nRcvBufferLen = 80 * 1024;
                int nSndBufferLen = 1 * 1024 * 1024;
                int nLen = sizeof(int);

                setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *) &nSndBufferLen, nLen);
                setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *) &nRcvBufferLen, nLen);
                int flag;
                flag = fcntl(sock, F_GETFL);
                fcntl(sock, F_SETFL, flag | O_NONBLOCK);

                evReg.data.fd = sock;
                evReg.events = EPOLLIN | EPOLLONESHOT;
                this->sessions[sock]->sessionId = sock;
                this->sessions[sock]->rrindex = rrIndex++;
                epoll_ctl(this->epolls[rrIndex % this->maxWorker], EPOLL_CTL_ADD, sock, &evReg);



            }
        }
    }

    close(lisEpfd);
}


int Poller::run(int port) {
    epolls.resize(maxWorker);
    for (int i = 0; i < this->maxWorker; i++)
        this->taskQueue.emplace_back(moodycamel::ConcurrentQueue<sockInfo>());


    int epi;
    for (epi = 0; epi < this->maxWorker; ++epi) {
        epolls[epi] = epoll_create(20);
    }

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
        perror("bind");
        return -1;
    }

    ::listen(lisSock, 4096);


    int i;

    for (i = 0; i < this->maxWorker; ++i) {
        worker.emplace_back(Poller::workerThreadCB, this, i);
    }

    listen_thread = std::thread(Poller::listenThreadCB, this, port);

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