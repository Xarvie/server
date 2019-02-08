/*
 * Spider.cpp
 *
 *  Created on: Jan 19, 2019
 *      Author: xarvie
 */

#include "DefConfig.h"
#ifdef OS_LINUX
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <iostream>
#include <list>
#include <vector>
#include <thread>
#include <utility>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>

#include "concurrentqueue.h"
#include "Spider.h"


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
	connectSockInfo.task = 3;
	connectSockInfo.rrindex = this->rrindex;
	connectSockInfo.fd = this->sock;
	this->spider->acceptTaskQueue[this->rrindex % EPOLL_NUM].enqueue(connectSockInfo);
}

Spider::Spider(int port)
{
	this->m_conn_table.resize(CONN_MAXFD);
	int c;
	for (c = 0; c < CONN_MAXFD; ++c)
	{
		m_conn_table[c].sock = c;
		m_conn_table[c].spider = this;
		m_conn_table[c].cbAlloc();
	}

	for(int i = 0; i < EPOLL_NUM; i++)
		this->acceptTaskQueue.emplace_back(moodycamel::ConcurrentQueue<sockInfo>());
	init_thread = std::thread(initThreadCB, this, port);
}

Spider::~Spider()
{
    init_thread.join();
	sockInfo taskInfo;
	taskInfo.task = Spider::REQ_SHUTDOWN;

	this->listenTaskQueue.enqueue(taskInfo);

	for(int i = 0; i < EPOLL_NUM; i++)
	{
		this->acceptTaskQueue[i].enqueue(taskInfo);
	}

	int i = 0,c = 0;

	for (i = 0; i < EPOLL_NUM; ++i)
	{
		if(worker[i].joinable())
			worker[i].join();
	}
	if(listen_thread.joinable())
		listen_thread.join();

	struct epoll_event evReg;

	for (c = 0; c < CONN_MAXFD; ++c)
	{
		connection* conn = &m_conn_table[c];
		if (conn->type)
		{
			epoll_ctl(epfd[conn->rrindex % EPOLL_NUM], EPOLL_CTL_DEL, conn->sock, &evReg);
			close(conn->sock);
		}
	}

	for (int epi = 0; epi < EPOLL_NUM; ++epi)
	{
		close(epfd[epi]);
	}
	close(lisSock);
}

class connection;
extern int send(int fd, char *data, int len);

class connector
{
	const unsigned MAX_THREAD_NUM = 7;
	void init_threads();
	void loop();

public:
	std::vector<std::thread> threads;

};
void connectorThread(void *arg)
{
	moodycamel::ConcurrentQueue<int> x;

}
void connector::init_threads()
{

}


int Spider::send(int fd, char *data, int len)
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

int Spider::handleReadEvent(connection* conn)
{
	char buff[BUFFER_SIZE + 1];
	int ret = read(conn->sock, buff, BUFFER_SIZE);

	if (ret > 0)
	{
		conn->readBuffer.push_back(ret, buff);
		const int buffSize = *(int*) conn->readBuffer.buff;
		if (conn->readBuffer.size >= 4 && buffSize <= conn->readBuffer.size)
		{
			Msg msg;
			msg.fd = conn->sock;
			msg.buffer = conn->readBuffer;
			while(true)
			{
				if(this->msgQueue.enqueue(msg))
					break;
			}
			conn->readBuffer.erase(conn->readBuffer.size);
		}

	}
	else if (ret == 0)
	{
		return -1;
	}
	else
	{
		if (errno != EINTR && errno != EAGAIN)
		{
			return -1;
		}
	}

	return 0;
}

int Spider::handleWriteEvent(connection* conn)
{
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
	}
	else
	{
		conn->writeBuffer.erase(ret);
	}

	return 0;
}
#if defined(OS_LINUX)
void Spider::closeConnection(connection* conn)
{

	struct epoll_event evReg;
	close(conn->sock);
	conn->type = 0;
	conn->readBuffer.erase(conn->readBuffer.size);
	conn->writeBuffer.erase(conn->writeBuffer.size);
	epoll_ctl(epfd[conn->rrindex % EPOLL_NUM], EPOLL_CTL_DEL, conn->sock, &evReg);
}
#endif

void Spider::workerThreadCB(Spider* thisPtr/*TODO bug?*/, int *fd, int epindex)
{
	thisPtr->workerThread(fd, epindex);
}
void Spider::workerThread(int *epollfd, int epindex)
{
	int epfd = epollfd[epindex];

	struct epoll_event event;
	struct epoll_event evReg;

	while (true)
	{
		int numEvents = epoll_wait(epfd, &event, 1, 1000);//TODO wait 1
		sockInfo taskInfo ;
		int shutdown = 0;
		while(true)
		{
			bool ret = this->acceptTaskQueue[epindex].try_dequeue(taskInfo);
			if(ret == false)
				break;

			if(taskInfo.task == REQ_DISCONNECT)
			{
				connection* conn = &this->m_conn_table[taskInfo.fd];
				if(conn->rrindex == taskInfo.rrindex)
					closeConnection(conn);
			}
			if(taskInfo.task == REQ_SHUTDOWN)
			{
				shutdown = 1;
				break;
			}

		}
		if(shutdown)
		{
			break;
		}

		if (numEvents == -1)
		{
			//printf("wait\n %d", errno);
		}

		if (numEvents > 0)
		{
			int sock = event.data.fd;
			connection* conn = &this->m_conn_table[sock];
			if(conn->type == 0)
				continue;
			if (event.events & EPOLLOUT)
			{
				if (this->handleWriteEvent(conn) == -1)
				{
					this->closeConnection(conn);
					continue;
				}
			}

			if (event.events & EPOLLIN)
			{
				if (this->handleReadEvent(conn) == -1)
				{
					this->closeConnection(conn);
					continue;
				}
			}

			evReg.events = EPOLLIN | EPOLLONESHOT;
			if (conn->writeBuffer.size > 0)
				evReg.events |= EPOLLOUT;
			evReg.data.fd = sock;
			epoll_ctl(epfd, EPOLL_CTL_MOD, conn->sock, &evReg);
		}
	}
}

void Spider::listenThreadCB(Spider* thisPtr/*TODO bug?*/, void * arg)
{
	thisPtr->listenThread(arg);
}

int Spider::listen(const int port)
{
	sockInfo connectSockInfo;
	connectSockInfo.port = port;
	connectSockInfo.task = 1;
	listenTaskQueue.enqueue(connectSockInfo);
	return 0;
}

void Spider::listenThread(void * arg)
{
	int lisEpfd = epoll_create(5);

	struct epoll_event evReg;
	evReg.events = EPOLLIN;
	evReg.data.fd = this->lisSock;


	epoll_ctl(lisEpfd, EPOLL_CTL_ADD, this->lisSock, &evReg);

	struct epoll_event event;

	unsigned short rrIndex = 0; /* round robin rrindex */

	while (true)
	{
		int numEvent = epoll_wait(lisEpfd, &event, 1, 1000);

		//TODO con

		std::vector<sockInfo> sockInfoVec;
		int shutdown = 0;
		while(true)
		{
			sockInfo deque;
			bool success = this->listenTaskQueue.try_dequeue(deque);
			if(!success)
				break;

			if(deque.task == REQ_SHUTDOWN)
			{
				shutdown = 1;
			}
			if(deque.task == REQ_CONNECT)
			{
				int sock = deque.fd;
				if (sock > 0)
				{

					this->m_conn_table[sock].type = 1;
					int nodelay = 1;
					if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay,
								   sizeof(nodelay)) < 0)
						perror("error: nodelay");


					int flag;
					flag = fcntl(sock, F_GETFL);
					fcntl(sock, F_SETFL, flag | O_NONBLOCK);

					evReg.data.fd = sock;
					evReg.events = EPOLLIN | EPOLLONESHOT;

					this->m_conn_table[sock].rrindex = rrIndex++;
					epoll_ctl(this->epfd[rrIndex], EPOLL_CTL_ADD, sock, &evReg);
					deque.event = 2;
					eventQueue.enqueue(deque);
				}
			}
		}
		if(shutdown)
		{
			break;
		}

		if (numEvent > 0)
		{
			sockInfo deque;
			int sock = accept(this->lisSock, NULL, NULL);
			if (sock > 0)
			{
				this->m_conn_table[sock].type = 1;

				int nodelay = 1;
				if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay,
						sizeof(nodelay)) < 0)
					perror("error: nodelay");

				int flag;
				flag = fcntl(sock, F_GETFL);
				fcntl(sock, F_SETFL, flag | O_NONBLOCK);

				evReg.data.fd = sock;
				evReg.events = EPOLLIN | EPOLLONESHOT;

				this->m_conn_table[sock].rrindex = rrIndex++;
				epoll_ctl(this->epfd[rrIndex % EPOLL_NUM], EPOLL_CTL_ADD, sock, &evReg);


				deque.event = 1;
				eventQueue.enqueue(deque);
			}
		}
	}

	close(lisEpfd);
}

int Spider::idle()
{
	return 0;
}

int Spider::loop(int socketFd, const char * ip, const short port)
{
	int epfd = epoll_create(5);

	struct epoll_event event;
	struct epoll_event evReg;

	while (true)
	{
		int numEvents = epoll_wait(epfd, &event, 1, 1000);
		idle();
		if (numEvents == -1)
		{
			printf("wait\n %d", errno);
		}

		if (numEvents > 0)
		{
			int sock = event.data.fd;
			connection* conn = &m_conn_table[sock];

			if (event.events & EPOLLOUT)
			{
				if (handleWriteEvent(conn) == -1)
				{
					closeConnection(conn);
					continue;
				}
			}

			if (event.events & EPOLLIN)
			{
				if (handleReadEvent(conn) == -1)
				{
					closeConnection(conn);
					continue;
				}
			}

			evReg.events = EPOLLIN | EPOLLONESHOT;
			if (conn->writeBuffer.size > 0)
				evReg.events |= EPOLLOUT;
			evReg.data.fd = sock;
			epoll_ctl(epfd, EPOLL_CTL_MOD, conn->sock, &evReg);
		}
	}
	return 0;
}

int Spider::initThreadCB(Spider* self, int port)
{
	self->init(port);
	return 0;
}

int Spider::init(int port)
{
	int epi;
	for (epi = 0; epi < EPOLL_NUM; ++epi)
	{
		epfd[epi] = epoll_create(20);
	}

	lisSock = socket(AF_INET, SOCK_STREAM, 0);

	int reuse = 1;
	setsockopt(lisSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	int flag;
	flag = fcntl(lisSock, F_GETFL);
	fcntl(lisSock, F_SETFL, flag | O_NONBLOCK);

	struct sockaddr_in lisAddr;
	lisAddr.sin_family = AF_INET;
	lisAddr.sin_port = htons(port);
	lisAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(lisSock, (struct sockaddr *) &lisAddr, sizeof(lisAddr)) == -1)
	{
		perror("bind");
		return -1;
	}

	::listen(lisSock, 4096);



	int i;

	for (i = 0; i < EPOLL_NUM; ++i)
	{
		worker.emplace_back(Spider::workerThreadCB, this, epfd, i);
	}

	listen_thread = std::thread(Spider::listenThreadCB, this, nullptr);

	return 0;
}

bool Spider::get(Msg& msg)
{
    return this->msgQueue.try_dequeue(msg);
}

void Spider::disconnect(int fd)
{
    m_conn_table[fd].disconnect();
}

int Spider::connect(const char * ip, short port)
{
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);

    sockInfo connectSockInfo;
    connectSockInfo.ip = ip;
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

#endif