#include "SystemReader.h"

#if defined(OS_DARWIN) && !defined(SELECT_SERVER)

#include "KqueuePoller.h"

#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

int IsEagain() {
#if defined(OS_WINDOWS)
    int err = getSockError();
    if (err == EINTR || err == EAGAIN  || err == EWOULDBLOCK || err == WSAEWOULDBLOCK) {
        return 1;
    }
    return 0;
#else
    int err = errno;
    if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK) {
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

int Session::cbRead(int readNum) {
    return 0;
}

int Session::cbAlloc() {
    return 0;
}

int Session::disconnect() {
    sockInfo connectSockInfo;
    connectSockInfo.task = Poller::REQ_DISCONNECT;
    connectSockInfo.fd = (int) this->sessionId;
    return 0;
}

Poller::~Poller() {
    sockInfo connectSockInfo;
    connectSockInfo.task = Poller::REQ_SHUTDOWN;
    for (int i = 0; i < 1/*todo EPOLL_NUM*/; i++) {
        this->acceptTaskQueue[i].enqueue(connectSockInfo);
    }
    if (this->listenThread.joinable()) {
        this->listenThread.join();
    }
    for (int c = 0; c < CONN_MAXFD; ++c) {
        Session *conn = sessions[c];
        if (conn->type) {
            close(conn->sessionId);
        }
        sessions[c]->readBuffer.destroy();
        sessions[c]->writeBuffer.destroy();
    }
    close(lisSock);
}

struct event_data {
    char buffer[BUFFER_SIZE];
    int buffer_read;
    int buffer_write;
    Poller *this_ptr;
    int type;

    int (*on_read)(struct event_data *self, struct kevent *event);

    int (*on_write)(struct event_data *self, struct kevent *event);
};

void Poller::workerThreadCB(int pollerIndex) {
    int nev = 0;
    bool dequeueRet = false;
    sockInfo event1;
    moodycamel::ConcurrentQueue<sockInfo> &queue = taskQueue[pollerIndex];

    while (1) {
        while (queue.try_dequeue(event1)) {
            if (event1.event == ACCEPT_EVENT) {
                int ret = 0;
                int clientFd = event1.fd;

                {
                    int nodelay = 1;
                    if (setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                                   sizeof(nodelay)) < 0)
                        perror("error: nodelay");

                    int nRcvBufferLen = 80 * 1024;
                    int nSndBufferLen = 1 * 1024 * 1024;
                    int nLen = sizeof(int);

                    setsockopt(clientFd, SOL_SOCKET, SO_SNDBUF, (char *) &nSndBufferLen, nLen);
                    setsockopt(clientFd, SOL_SOCKET, SO_RCVBUF, (char *) &nRcvBufferLen, nLen);
                    this->sessions[clientFd]->type = 1;
                    this->sessions[clientFd]->sessionId = (uint64_t) clientFd;
                    int flags = fcntl(clientFd, F_GETFL, 0);
                    if (flags < 0) on_error("Could not get client socket flags: %s\n", strerror(errno));

                    int err = fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
                    if (err < 0) on_error("Could not set client socket to be non blocking: %s\n", strerror(errno));
                    int pollerIndex = clientFd % this->maxWorker;
                    EV_SET(&event_set[pollerIndex], clientFd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                    if (kevent(this->queue[pollerIndex], &event_set[pollerIndex], 1, NULL, 0, NULL) == -1) {
                        printf("error\n");
                    }
                    EV_SET(&event_set[pollerIndex], clientFd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
                    if (kevent(this->queue[pollerIndex], &event_set[pollerIndex], 1, NULL, 0, NULL) == -1) {
                        printf("error\n");
                    }
                }
            }
        }
        struct timespec timeout;
        timeout.tv_sec = 1;
        timeout.tv_nsec = 0;
        nev = kevent(this->queue[pollerIndex], NULL, 0, event_list[pollerIndex], 32, &timeout);
        if (nev == 0) {

            continue;
        } else if (nev < 0) {
            std::cout << "kevent < 0" << std::endl;
            return;
        }
        for (int event = 0; event < nev; event++) {
            if (event_list[pollerIndex][event].flags & EV_EOF) {

            }
            if (event_list[pollerIndex][event].flags & EVFILT_READ) {
                if (-1 == this->on_read(&event_list[pollerIndex][event])) {
                    int sock = event_list[pollerIndex][event].ident;
                    Session *conn = this->sessions[sock];
                    this->closeSession(conn);
                }

            }
            if (event_list[pollerIndex][event].flags & EVFILT_WRITE) {
                int sock = event_list[pollerIndex][event].ident;
                Session *conn = this->sessions[sock];
                if (-1 == this->on_write(&event_list[pollerIndex][event])) {
                    this->closeSession(conn);
                } else{
                    EV_SET(&event_set[pollerIndex], conn->sessionId, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
                }
            }
        }
    }
}

void Poller::listenThreadCB(int port) {
    int err, flags;

    struct sockaddr client;
    socklen_t client_len = sizeof(client);

    while (true) {
        int client_fd = accept(this->lisSock, &client, &client_len);
        if (client_fd < 0) {
            on_error("Accept failed (should this be fatal?): %s\n", strerror(errno));
        }
        int pollerId = 0;
#ifdef OS_WINDOWS
        pollerId = client_fd / 4 % maxWorker;
#else
        pollerId = client_fd % maxWorker;
#endif
        sockInfo x;
        x.fd = client_fd;
        x.event = ACCEPT_EVENT;
        taskQueue[pollerId].enqueue(x);
    }


}

void Poller::event_change(int ident, int filter, int flags, void *udata) {
    struct kevent *e;

    if (events_alloc == 0) {
        events_alloc = 64;
        events = (struct kevent *) malloc(events_alloc * sizeof(struct kevent));
    }
    if (events_alloc <= events_used) {
        events_alloc *= 2;
        events = (struct kevent *) realloc(events, events_alloc * sizeof(struct kevent));
    }

    int index = events_used++;
    e = &events[index];

    e->ident = ident;
    e->filter = filter;
    e->flags = flags;
    e->fflags = 0;
    e->data = 0;
    e->udata = udata;
}


int Poller::on_read(struct kevent *event) {


    int sock = event->ident;
    Session *conn = this->sessions[sock];
    if (conn->readBuffer.size < 0)
        return -1;
    unsigned char *buff = conn->readBuffer.buff + conn->readBuffer.size;

    int ret = recv(conn->sessionId, buff, conn->readBuffer.capacity - conn->readBuffer.size, 0);
    if (ret > 0) {
        conn->readBuffer.size += ret;
        conn->readBuffer.alloc();
        if (conn->readBuffer.size > 1024 * 1024 * 3) {
            return -1;
            //TODO close socket
        }
        //TODO
        int readBytes = onReadMsg(conn->sessionId, ret);
        conn->readBuffer.size -= readBytes;
        if (conn->readBuffer.size < 0)
            return -1;
    } else if (ret == 0) {
        return -1;
    } else {
        if (errno != EWOULDBLOCK && errno != EAGAIN) return -1;
    }

    return 0;
}

int Poller::on_write(struct kevent *event) {

    int sock = event->ident;
    int pollerIndex = sock % this->maxWorker;
    Session *conn = sessions[sock];
    if (conn->writeBuffer.size == 0)
        return 0;

    if (conn->writeBuffer.size < 0)
        return -1;


    int ret = send(conn->sessionId, (void *) conn->writeBuffer.buff,
                   conn->writeBuffer.size, 0);

    if (ret == -1) {
        if (errno != EINTR && errno != EAGAIN) {
            return -1;
        }
    }else if(ret == 0){
        //TODO
    }
    else {
        onWriteBytes(conn->sessionId, ret);
        conn->writeBuffer.erase(ret);
    }
    return 0;
}

void Poller::closeSession(Session *conn) {

    if (conn->readBuffer.size < 0 || conn->writeBuffer.size < 0)
        return;
    int index = conn->sessionId % this->maxWorker;
    linger lingerStruct;

    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    setsockopt(conn->sessionId, SOL_SOCKET, SO_LINGER,
               (char *) &lingerStruct, sizeof(lingerStruct));
    conn->readBuffer.size = -1;
    conn->writeBuffer.size = -1;
    EV_SET(&event_set[index], conn->sessionId, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&event_set[index], conn->sessionId, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

    closeSocket(conn->sessionId);
}

void Poller::disconnect(int fd) {
    sessions[fd]->disconnect();
}

int Poller::run(int port) {
    signal(SIGPIPE, SIG_IGN);

    taskQueue.resize(this->maxWorker);
    for (int i = 0; i < 1/*todo EPOLL_NUM*/; i++)
        this->acceptTaskQueue.emplace_back(moodycamel::ConcurrentQueue<sockInfo>());
    {

        int err, flags;


        for (int i = 0; i < this->maxWorker; i++) {
            this->queue.push_back(kqueue());
            if (this->queue[i] < 0) on_error("Could not create kqueue: %s\n", strerror(errno));
            event_list.push_back((struct kevent *) xmalloc(1024 * sizeof(struct kevent)));

        }
        event_set.resize(this->maxWorker);

        this->lisSock = socket(AF_INET, SOCK_STREAM, 0);
        if (this->lisSock < 0) on_error("Could not create server socket: %s\n", strerror(errno))
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        server.sin_addr.s_addr = htonl(INADDR_ANY);

        int opt_val = 1;
        setsockopt(this->lisSock, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

        err = bind(this->lisSock, (struct sockaddr *) &server, sizeof(server));
        if (err < 0) on_error("Could not bind server socket: %s\n", strerror(errno));

        flags = fcntl(this->lisSock, F_GETFL, 0);
        if (flags < 0) on_error("Could not get server socket flags: %s\n", strerror(errno))

        // err = fcntl(this->lisSock, F_SETFL, flags | O_NONBLOCK);
        //if (err < 0) on_error("Could set server socket to be non blocking: %s\n", strerror(errno));

        err = ::listen(this->lisSock, SOMAXCONN);
        if (err < 0) on_error("Could not listen: %s\n", strerror(errno));

    }

    for (int i = 0; i < this->maxWorker; i++) {
        workerThreads.emplace_back(std::thread([=] { this->workerThreadCB(i); }));//TODO
    }

    listenThread = std::thread([=] { this->listenThreadCB(port); });//TODO

    listenThread.join();
    for (auto &E:workerThreads) {
        E.join();
    }

    return 0;
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
    listenTaskQueue.enqueue(connectSockInfo);
    return 0;
}


int Poller::sendMsg(uint64_t fd, const Msg &msg) {
    unsigned char *data = msg.buff;
    int len = msg.len;
    int pollerIndex = fd % this->maxWorker;
    Session *conn = this->sessions[fd];
    int leftBytes = 0;
    if (conn->writeBuffer.size > 0) {
        conn->writeBuffer.push_back(len, data);
        return 0;
    } else {
        int ret = send(conn->sessionId, data, len, 0);
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
    if (leftBytes > 0) {
        EV_SET(&event_set[pollerIndex], conn->sessionId, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
    }


    return 0;
}

#endif