/*
 * Spider.cpp
 *
 *  Created on: Jan 19, 2019
 *      Author: xarvie
 */
#include "DefConfig.h"
#ifdef OS_DARWIN
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <errno.h>
#include <arpa/inet.h>
#include "Buffer.h"
#include "Spider.h"

#define BUFFER_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

int Session::cbRead(int readNum)
{
    return 0;
}

int Session::cbAlloc()
{
    return 0;
}

int Session::disconnect()
{
    sockInfo connectSockInfo;
    connectSockInfo.task = Poller::REQ_DISCONNECT;
    connectSockInfo.rrindex = this->rrindex;
    connectSockInfo.fd = (int)this->sessionId;
    return 0;
}

Poller::~Poller()
{
    sockInfo connectSockInfo;
    connectSockInfo.task = Poller::REQ_SHUTDOWN;
    for(int i = 0; i < 1/*todo EPOLL_NUM*/; i++)
    {
        this->acceptTaskQueue[i].enqueue(connectSockInfo);
    }
    if(this->listen_thread.joinable())
    {
        this->listen_thread.join();
    }
    for (int c = 0; c < CONN_MAXFD; ++c)
    {
        Session* conn = sessions[c];
        if (conn->type)
        {
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
    Poller* this_ptr;
    int type;
    int (*on_read) (struct event_data *self, struct kevent *event);
    int (*on_write) (struct event_data *self, struct kevent *event);
};

void Poller::event_server_listen (int port) {
    int err, flags;

    this->queue = kqueue();
    if (this->queue < 0) on_error("Could not create kqueue: %s\n", strerror(errno));

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

    err = fcntl(this->lisSock, F_SETFL, flags | O_NONBLOCK);
    if (err < 0) on_error("Could set server socket to be non blocking: %s\n", strerror(errno));

    err = ::listen(this->lisSock, SOMAXCONN);
    if (err < 0) on_error("Could not listen: %s\n", strerror(errno));
}

void Poller::event_change(int ident, int filter, int flags, void *udata)
{
    struct kevent *e;

    if (events_alloc == 0) {
        events_alloc = 64;
        events = (struct kevent *)malloc(events_alloc * sizeof(struct kevent));
    }
    if (events_alloc <= events_used) {
        events_alloc *= 2;
        events = (struct kevent *)realloc(events, events_alloc * sizeof(struct kevent));
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

void Poller::event_loop()
{
    int new_events;
    struct timespec tim={1,0};


    while (true)
    {
        new_events = kevent(queue, events, events_used, events, events_alloc, &tim);
        sockInfo taskInfo ;
        int epindex = 0;
        bool ret = this->acceptTaskQueue[epindex].try_dequeue(taskInfo);
        if(ret)
        {
            if(taskInfo.task == REQ_DISCONNECT)
            {
                Session* conn = this->sessions[taskInfo.fd];
                if(conn->rrindex == taskInfo.rrindex && conn->type != 0)
                    closeConnection(conn);
            }
            if(taskInfo.task == REQ_SHUTDOWN)
            {
                break;
            }
        }
        if (new_events < 0)
        {
            continue;
        }
        events_used = 0;

        for (int i = 0; i < new_events; i++) {
            struct kevent *e = &events[i];
            struct event_data *udata = (struct event_data *) e->udata;


            if (udata == NULL) continue;
            switch (udata->type)
            {
                case ACCEPT_EVENT:
                {
                    if(e->filter == EVFILT_READ)
                    {
                        while(udata->this_ptr->event_on_accept(udata, e));
                    }
                    break;
                }
                case RW_EVENT:
                {
                    if(e->filter == EVFILT_READ)
                    {
                        while(udata->this_ptr->event_on_read(udata, e));
                    }
                    if(e->filter == EVFILT_WRITE)
                    {
                        while (udata->this_ptr->event_on_write(udata, e));
                    }
                    break;
                }

                default:
                {
                    break;
                }
            }
        }
    }
}

int Poller::event_flush_write(struct event_data *self, struct kevent *event) {

    int sock = event->ident;
    //todo read到close之后是否有write事件?rrindex判断
    Session* conn = sessions[sock];
    if (conn->writeBuffer.size == 0)
        return 0;

    int ret = write(conn->sessionId, (void*) conn->writeBuffer.buff,
                    conn->writeBuffer.size);

    if (ret == -1)
    {
        if (errno != EINTR && errno != EAGAIN)
        {
            return -1;
        }
        event_change(conn->sessionId, EVFILT_WRITE, EV_ENABLE, self);
    }
    else
    {
        conn->writeBuffer.erase(ret);
    }
    if(conn->writeBuffer.size == 0)
        event_change(conn->sessionId, EVFILT_WRITE, EV_DISABLE, self);
    return 0;
}

void Poller::closeConnection(Session* conn)
{

    //struct epoll_event evReg;
    close(conn->sessionId);
    conn->type = 0;
    conn->readBuffer.erase(conn->readBuffer.size);
    conn->writeBuffer.erase(conn->writeBuffer.size);
    //epoll_ctl(epfd[conn->rrindex % EPOLL_NUM], EPOLL_CTL_DEL, conn->sock, &evReg);
}

int Poller::event_on_read(struct event_data *self, struct kevent *event){

    int sock = event->ident;
    Session* conn = this->sessions[sock];
    unsigned char buff[BUFFER_SIZE];
    int ret = read(conn->sessionId, buff, BUFFER_SIZE - 1);

    if (ret > 0)
    {
        //TODO
        buff[ret] = 0;
        Msg msg;
        msg.len = ret;
        msg.buff = buff;
        onReadMsg(conn->sessionId, msg);
    }

    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
        this->closeConnection(conn);
        free(self);
        return 0;
    }

    if (ret == 0) {
        this->closeConnection(conn);
        free(self);
        return 0;
    }

    return 0;
}

int Poller::event_on_write (struct event_data *self, struct kevent *event) {

    return event_flush_write(self, event);
}

int Poller::event_on_accept(struct event_data *self, struct kevent *event) {
    struct sockaddr client;
    socklen_t client_len = sizeof(client);

    int client_fd = accept(this->lisSock, &client, &client_len);
    int flags;
    int err;

    if (client_fd < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
        on_error("Accept failed (should this be fatal?): %s\n", strerror(errno));
    }
    this->sessions[client_fd]->type = 1;
    this->sessions[client_fd]->rrindex = rrIndex++;
    this->sessions[client_fd]->sessionId = (uint64_t)client_fd;
    flags = fcntl(client_fd, F_GETFL, 0);
    if (flags < 0) on_error("Could not get client socket flags: %s\n", strerror(errno));

    err = fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    if (err < 0) on_error("Could not set client socket to be non blocking: %s\n", strerror(errno));

    this->sessions[client_fd]->client_data = (struct event_data *) malloc(sizeof(struct event_data));
    this->sessions[client_fd]->client_data->this_ptr = this;//TODO

    this->sessions[client_fd]->client_data->type = RW_EVENT;
    event_change(client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, this->sessions[client_fd]->client_data);
    event_change(client_fd, EVFILT_WRITE, EV_ADD | EV_DISABLE, this->sessions[client_fd]->client_data);

    return 1;
}

bool Poller::get(Msg& msg)
{
    return this->msgQueue.try_dequeue(msg);
}

void Poller::disconnect(int fd)
{
    sessions[fd]->disconnect();
}

int Poller::run(int port)
{
    signal(SIGPIPE, SIG_IGN);

    for(int i = 0; i < 1/*todo EPOLL_NUM*/; i++)
        this->acceptTaskQueue.emplace_back(moodycamel::ConcurrentQueue<sockInfo>());

    //listen_thread = std::thread(initThreadCB, this, port);//TODO


    struct event_data server = {
            .type = ACCEPT_EVENT,
            .this_ptr  = this //TODO
    };

    this->event_server_listen(port);
    this->event_change(this->lisSock, EVFILT_READ, EV_ADD | EV_ENABLE, &server);
    this->event_loop();

    return 0;
}

int Poller::connect(const char * ip, short port)
{
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
    if (ret != 0)
    {
        close(socketFd);
        return false;
    }
    listenTaskQueue.enqueue(connectSockInfo);
    return 0;
}


int Poller::sendMsg(uint64_t fd, const Msg &msg) {
    unsigned char* data = msg.buff;
    int len = msg.len;
    Session* conn = this->sessions[fd];
    int leftBytes = 0;
    if (conn->writeBuffer.size > 0)
    {
        conn->writeBuffer.push_back(len, data);
        return 0;
    }
    else
    {
        int ret = write(conn->sessionId, data, len);
        if (ret > 0)
        {
            if (ret == len)
                return 0;

            leftBytes = len - ret;
            conn->writeBuffer.push_back(leftBytes, data + ret);
        }
        else
        {
            if (errno != EINTR && errno != EAGAIN)
                return -1;

            leftBytes = len;
            conn->writeBuffer.push_back(len, data);
        }
    }
    if(leftBytes>0)
        event_change(conn->sessionId, EVFILT_WRITE, EV_ADD | EV_ENABLE, this->sessions[conn->sessionId]->client_data);

    return 0;
}

#endif