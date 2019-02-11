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


#include "darwinSpider.h"
#include "Buffer.h"

#define BUFFER_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

int connection::cbRead(int readNum)
{
    return 0;
}

int connection::cbAlloc()
{
    return 0;
}

int connection::disconnect()
{
    sockInfo connectSockInfo;
    connectSockInfo.task = Spider::REQ_DISCONNECT;
    connectSockInfo.rrindex = this->rrindex;
    connectSockInfo.fd = this->sock;
    this->spider->acceptTaskQueue[this->rrindex % EPOLL_NUM].enqueue(connectSockInfo);
    return 0;
}

Spider::Spider(int port)
{
    this->m_conn_table.resize(CONN_MAXFD);
    for (int c = 0; c < CONN_MAXFD; ++c)
    {
        m_conn_table[c].sock = c;
        m_conn_table[c].spider = this;
        m_conn_table[c].readBuffer.init();
        m_conn_table[c].writeBuffer.init();
        m_conn_table[c].cbAlloc();
    }
    for(int i = 0; i < 1/*todo EPOLL_NUM*/; i++)
        this->acceptTaskQueue.emplace_back(moodycamel::ConcurrentQueue<sockInfo>());

    listen_thread = std::thread(initThreadCB, this, port);
}

Spider::~Spider()
{
    sockInfo connectSockInfo;
    connectSockInfo.task = Spider::REQ_SHUTDOWN;
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
        connection* conn = &m_conn_table[c];
        if (conn->type)
        {
            close(conn->sock);
        }
        m_conn_table[c].readBuffer.destroy();
        m_conn_table[c].writeBuffer.destroy();
    }
    close(lisSock);
}

int Spider::initThreadCB(Spider* self, int port)
{
    self->start1(port);
    return 0;
}

struct event_data {
    char buffer[BUFFER_SIZE];
    int buffer_read;
    int buffer_write;
    Spider* this_ptr;
    int type;
    int (*on_read) (struct event_data *self, struct kevent *event);
    int (*on_write) (struct event_data *self, struct kevent *event);
};

void Spider::event_server_listen (int port) {
    int err, flags;

    this->queue = kqueue();
    if (this->queue < 0) on_error("Could not create kqueue: %s\n", strerror(errno));

    this->lisSock = socket(AF_INET, SOCK_STREAM, 0);
    if (this->lisSock < 0) on_error("Could not create server socket: %s\n", strerror(errno))

    this->server.sin_family = AF_INET;
    this->server.sin_port = htons(port);
    this->server.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt_val = 1;
    setsockopt(this->lisSock, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

    err = bind(this->lisSock, (struct sockaddr *) &this->server, sizeof(this->server));
    if (err < 0) on_error("Could not bind server socket: %s\n", strerror(errno));

    flags = fcntl(this->lisSock, F_GETFL, 0);
    if (flags < 0) on_error("Could not get server socket flags: %s\n", strerror(errno))

    err = fcntl(this->lisSock, F_SETFL, flags | O_NONBLOCK);
    if (err < 0) on_error("Could set server socket to be non blocking: %s\n", strerror(errno));

    err = ::listen(this->lisSock, SOMAXCONN);
    if (err < 0) on_error("Could not listen: %s\n", strerror(errno));
}

void Spider::event_change(int ident, int filter, int flags, void *udata)
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

void Spider::event_loop()
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
                connection* conn = &this->m_conn_table[taskInfo.fd];
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

int Spider::event_flush_write(struct event_data *self, struct kevent *event) {

    int sock = event->ident;
    //todo read到close之后是否有write事件?rrindex判断
    connection* conn = &m_conn_table[sock];
    if (conn->writeBuffer.size == 0)
        return 0;

    int ret = write(conn->sock, (void*) conn->writeBuffer.buff,
                    conn->writeBuffer.size);

    if (ret == -1)
    {
        if (errno != EINTR && errno != EAGAIN)
        {
            return -1;
        }
        event_change(conn->sock, EVFILT_WRITE, EV_ENABLE, self);
    }
    else
    {
        conn->writeBuffer.erase(ret);
    }
    if(conn->writeBuffer.size == 0)
        event_change(conn->sock, EVFILT_WRITE, EV_DISABLE, self);
    return 0;
}

void Spider::closeConnection(connection* conn)
{

    //struct epoll_event evReg;
    close(conn->sock);
    conn->type = 0;
    conn->readBuffer.erase(conn->readBuffer.size);
    conn->writeBuffer.erase(conn->writeBuffer.size);
    //epoll_ctl(epfd[conn->rrindex % EPOLL_NUM], EPOLL_CTL_DEL, conn->sock, &evReg);
}

int Spider::event_on_read(struct event_data *self, struct kevent *event){

    int sock = event->ident;
    connection* conn = &this->m_conn_table[sock];
    char buff[BUFFER_SIZE];
    int ret = read(conn->sock, buff, BUFFER_SIZE);

    if (ret > 0)
    {
        conn->readBuffer.push_back(ret, buff);
        const int buffSize = *(int*) conn->readBuffer.buff;
        if (conn->readBuffer.size >= 4 && buffSize <= conn->readBuffer.size)
        {
            Msg msg;
            msg.buffer = conn->readBuffer;
            msg.buffer.buff = (char*)malloc((size_t)conn->readBuffer.capacity);
            msg.fd = conn->sock;
            memcpy(msg.buffer.buff,conn->readBuffer.buff, conn->readBuffer.size);


            while(true)
            {
                if(this->msgQueue.enqueue(msg))
                    break;
            }
            conn->readBuffer.erase(conn->readBuffer.size);
        }
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

int Spider::event_on_write (struct event_data *self, struct kevent *event) {

    return event_flush_write(self, event);
}

int Spider::event_on_accept(struct event_data *self, struct kevent *event) {
    struct sockaddr client;
    socklen_t client_len = sizeof(client);

    int client_fd = accept(this->lisSock, &client, &client_len);
    int flags;
    int err;

    if (client_fd < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
        on_error("Accept failed (should this be fatal?): %s\n", strerror(errno));
    }
    this->m_conn_table[client_fd].type = 1;
    this->m_conn_table[client_fd].rrindex = rrIndex++;

    flags = fcntl(client_fd, F_GETFL, 0);
    if (flags < 0) on_error("Could not get client socket flags: %s\n", strerror(errno));

    err = fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    if (err < 0) on_error("Could not set client socket to be non blocking: %s\n", strerror(errno));

    this->m_conn_table[client_fd].client_data = (struct event_data *) malloc(sizeof(struct event_data));
    this->m_conn_table[client_fd].client_data->this_ptr = this;//TODO

    this->m_conn_table[client_fd].client_data->type = RW_EVENT;
    event_change(client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, this->m_conn_table[client_fd].client_data);
    event_change(client_fd, EVFILT_WRITE, EV_ADD | EV_DISABLE, this->m_conn_table[client_fd].client_data);

    return 1;
}

bool Spider::get(Msg& msg)
{
    return this->msgQueue.try_dequeue(msg);
}

void Spider::disconnect(int fd)
{
    m_conn_table[fd].disconnect();
}

int Spider::start1(int port)
{
    struct event_data server = {
            .type = ACCEPT_EVENT,
            .this_ptr  = this //TODO
    };

    this->event_server_listen(port);
    this->event_change(this->lisSock, EVFILT_READ, EV_ADD | EV_ENABLE, &server);
    this->event_loop();

    return 0;
}

int Spider::connect(const char * ip, short port)
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


int Spider::_send(int fd, char *data, int len)
{
    connection* conn = &this->m_conn_table[fd];
    if (conn->writeBuffer.size > 0)
    {
        conn->writeBuffer.push_back(len, data);
        return 0;
    }
    else
    {
        int ret = write(conn->sock, data, len);
        if (ret > 0)
        {
            if (ret == len)
                return 0;

            int left = len - ret;
            conn->writeBuffer.push_back(left, data + ret);
        }
        else
        {
            if (errno != EINTR && errno != EAGAIN)
                return -1;

            conn->writeBuffer.push_back(len, data);
        }
    }

    return 0;
}

#endif