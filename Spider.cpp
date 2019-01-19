/*
 * Spider.cpp
 *
 *  Created on: Jan 19, 2019
 *      Author: xarvie
 */

#include "Spider.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <netinet/tcp.h>
#include <list>
#include <string>
#include <vector>
#include <thread>

#include "concurrentqueue.h"


Spider::Spider()
{
}

Spider::~Spider()
{
}

class connection;
extern int sendData(connection* conn, char *data, int len);

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


int Spider::sendData(connection* conn, char *data, int len)
{
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
			conn->woff = left;
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
	if (conn->roff == BUFFER_SIZE)
	{
		return -1;
	}
	char buff[BUFFER_SIZE + 1];
	int ret = read(conn->sock, buff, BUFFER_SIZE);

	if (ret > 0)
	{
		conn->readBuffer.push_back(ret, buff);
		const int buffSize = *(int*) buff;
		if (conn->readBuffer.size >= 4 && buffSize == conn->readBuffer.size)
		{
			conn->cbRead(ret);
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

void Spider::closeConnection(connection* conn)
{
	struct epoll_event evReg;

	conn->type = 0;
	conn->readBuffer.erase(conn->readBuffer.size);
	conn->writeBuffer.erase(conn->writeBuffer.size);
	epoll_ctl(epfd[conn->index], EPOLL_CTL_DEL, conn->sock, &evReg);
	close(conn->sock);
}

void Spider::workerThread(Spider* thisPtr/*TODO bug?*/, void *arg)
{
	int epfd = *(int *) arg;

	struct epoll_event event;
	struct epoll_event evReg;

	while (!thisPtr->shut_server)
	{
		int numEvents = epoll_wait(epfd, &event, 1, 1000);
		if (numEvents == -1)
		{
			printf("wait\n %d", errno);
		}

		if (numEvents > 0)
		{
			int sock = event.data.fd;
			connection* conn = &thisPtr->g_conn_table[sock];

			if (event.events & EPOLLOUT)
			{
				if (thisPtr->handleWriteEvent(conn) == -1)
				{
					thisPtr->closeConnection(conn);
					continue;
				}
			}

			if (event.events & EPOLLIN)
			{
				if (thisPtr->handleReadEvent(conn) == -1)
				{
					thisPtr->closeConnection(conn);
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

void Spider::listenThread(Spider* thisPtr/*TODO bug?*/, void * arg)
{
	int lisEpfd = epoll_create(5);

	struct epoll_event evReg;
	evReg.events = EPOLLIN;
	evReg.data.fd = thisPtr->lisSock;

	epoll_ctl(lisEpfd, EPOLL_CTL_ADD, thisPtr->lisSock, &evReg);

	struct epoll_event event;

	int rrIndex = 0; /* round robin index */

	while (!thisPtr->shut_server)
	{
		int numEvent = epoll_wait(lisEpfd, &event, 1, 1000);
		if (numEvent > 0)
		{
			int sock = accept(thisPtr->lisSock, NULL, NULL);
			if (sock > 0)
			{
				thisPtr->g_conn_table[sock].type = 1;

				int nodelay = 1;
				if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay,
						sizeof(nodelay)) < 0)
					perror("error: nodelay");

				int peer_buf_len = 1024;
				if (::setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &peer_buf_len,
						sizeof(peer_buf_len)) == -1)
				{
					perror("error: SO_SNDBUF");
				}
				if (::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &peer_buf_len,
						sizeof(peer_buf_len)) == -1)
				{
					perror("error: SO_RCVBUF");
				}

				int flag;
				flag = fcntl(sock, F_GETFL);
				fcntl(sock, F_SETFL, flag | O_NONBLOCK);

				evReg.data.fd = sock;
				evReg.events = EPOLLIN | EPOLLONESHOT;

				thisPtr->g_conn_table[sock].index = rrIndex;
				epoll_ctl(thisPtr->epfd[rrIndex], EPOLL_CTL_ADD, sock, &evReg);
				rrIndex = (rrIndex + 1) % EPOLL_NUM;
			}
		}
	}

	close(lisEpfd);
}

//int connect
int Spider::connect(const char * ip, const short port)
{
	int socketFd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in svraddr;
	svraddr.sin_family = AF_INET;
	if (strlen(ip))
		svraddr.sin_addr.s_addr = inet_addr(ip);
	else
		svraddr.sin_addr.s_addr = INADDR_ANY;
	if (svraddr.sin_addr.s_addr == INADDR_NONE)
	{
		return false;
	}
	svraddr.sin_port = htons(port);
	int ret = ::connect(socketFd, (struct sockaddr*) &svraddr, sizeof(svraddr));
	if (ret != 0 && errno != EINPROGRESS)
	{
		return false;
	}
	return true;
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

	while (!shut_server)
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
			connection* conn = &g_conn_table[sock];

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

int Spider::init()
{
	int c;
	for (c = 0; c < CONN_MAXFD; ++c)
	{
		g_conn_table[c].sock = c;
		g_conn_table[c].cbAlloc();
	}

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
	lisAddr.sin_port = htons(9876);
	lisAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(lisSock, (struct sockaddr *) &lisAddr, sizeof(lisAddr)) == -1)
	{
		perror("bind");
		return -1;
	}

	listen(lisSock, 4096);

	std::thread lisTid(Spider::listenThread, this, nullptr);

	int i;

	for (i = 0; i < EPOLL_NUM; ++i)
	{
		worker.push_back(std::thread(Spider::workerThread, this, epfd + i));
	}

	for (i = 0; i < EPOLL_NUM; ++i)
	{
		worker[i].join();
	}
	lisTid.join();

	struct epoll_event evReg;

	for (c = 0; c < CONN_MAXFD; ++c)
	{
		connection* conn = g_conn_table + c;
		if (conn->type)
		{
			epoll_ctl(epfd[conn->index], EPOLL_CTL_DEL, conn->sock, &evReg);
			close(conn->sock);
		}
	}

	for (epi = 0; epi < EPOLL_NUM; ++epi)
	{
		close(epfd[epi]);
	}
	close(lisSock);

	return 0;
}
