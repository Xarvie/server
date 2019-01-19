/*
 * Spider.h
 *
 *  Created on: Jan 19, 2019
 *      Author: xarvie
 */

#ifndef SERVER_SPIDER_H_
#define SERVER_ANT_H_
#include "Buffer.h"
#include <vector>

class connection
{
public:
	int sock;
	int index; /* which epoll fd this conn belongs to*/
	int type; /*0:null 1:accept 2:connect*/
#define BUFFER_SIZE 4096
	int roff;
	char rbuf[BUFFER_SIZE];
	int woff;
	char wbuf[BUFFER_SIZE];
	char* buff;
	int capacity;

	bool writeEagain = 0;
	int wirte_msg_size = 0;

	MessageBuffer writeBuffer;
	MessageBuffer readBuffer;
	const int suggested_capacity = BUFFER_SIZE;

	virtual int cbRead(int readNum)
	{
		//sendData(this, this->readBuffer.buff, this->readBuffer.size);
		return 0;
	}
	virtual int cbAlloc()
	{
		this->buff = (char*) malloc(connection::suggested_capacity);
		this->capacity = connection::suggested_capacity;
		return 0;
	}
};

class Spider
{
public:
	Spider();
	virtual ~Spider();

	int sendData(connection* conn, char *data, int len);
	int handleReadEvent(connection* conn);
	int handleWriteEvent(connection* conn);
	void closeConnection(connection* conn);
	static void workerThread(Spider* thisPtr, void *arg);
	static void listenThread(Spider* thisPtr, void * arg);
	int connect(const char * ip, const short port);
	int idle();
	int loop(int socketFd, const char * ip, const short port);
	int init();


#define CONN_MAXFD 65536
connection g_conn_table[CONN_MAXFD];

sig_atomic_t shut_server = 0;


#define EPOLL_NUM 4
int epfd[EPOLL_NUM];
int lisSock;

std::vector<std::thread> worker;

};

#endif /* SERVER_SPIDER_H_ */
