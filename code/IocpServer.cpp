
#include "config.h"
#ifdef OS_WINDOWS
#pragma warning (disable:4127)

#ifdef _IA64_
#pragma warning(disable:4267)
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define xmalloc malloc
#define xfree   free

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include <thread>
#include "IocpServer.h"
#include <vector>
#include <mutex>
#include "Spider.h"
#include "queue.h"

PER_SOCKET_CONTEXT *g_pCtxtList = NULL;        //上下文链表
std::recursive_mutex *xxx;

int myprintf(const char *lpFormat, ...);

int x = 0;

void Poller::sendMsg(uint64_t sessionId, const Msg &msg) {
    if (sessions[sessionId]->writeBuffer.size > 0) {
        sessions[sessionId]->writeBuffer.push_back(msg.len, msg.buff);
    } else {
        sessions[sessionId]->writeBuffer.push_back(msg.len, msg.buff);
        _send(sessionId, msg.buff, msg.len);
#ifdef OS_WINDOWS
        this->continueSendMsg(sessionId);
#endif
    }

}

DWORD Poller::WorkerThread(Poller *self, LPVOID WorkThreadContext) {

    HANDLE hIOCP = (HANDLE) WorkThreadContext;
    BOOL bSuccess = FALSE;
    int nRet = 0;
    LPWSAOVERLAPPED lpOverlapped = NULL;
    PER_SOCKET_CONTEXT *lpPerSocketContextNULL = NULL;
    PER_SOCKET_CONTEXT *lpPerSocketContext = NULL;
    WSABUF buffRecv;
    WSABUF buffSend;
    DWORD dwRecvNumBytes = 0;
    DWORD dwSendNumBytes = 0;
    DWORD dwFlags = 0;
    DWORD dwIoSize = 0;

    while (TRUE) {

        //
        // continually loop to service io completion packets
        //
        bSuccess = GetQueuedCompletionStatus(hIOCP, &dwIoSize,
                                             (PDWORD_PTR) &lpPerSocketContextNULL,
                                             (LPOVERLAPPED *) &lpOverlapped,
                                             INFINITE);
        if (!bSuccess)
            myprintf("GetQueuedCompletionStatus() failed: %d\n", GetLastError());
        lpPerSocketContext = (PER_SOCKET_CONTEXT *) lpOverlapped;
        if (lpPerSocketContext == NULL) {
            return (0);
        }

        if (self->g_bEndServer) {
            return (0);
        }

        if (!bSuccess || (bSuccess && (dwIoSize == 0))) {
            self->CloseClient(lpPerSocketContext, FALSE);
            continue;
        }

        //
        // determine what type of IO packet has completed by checking the PER_IO_CONTEXT
        // associated with this socket.  This will determine what action to take.
        //
        uint64_t sessionId = lpPerSocketContext->sessionId;
        switch (lpPerSocketContext->IOOperation) {
            case ClientIoRead: {
                Msg msg;
                self->sessions[sessionId]->iocp_context;
                msg.buff = (unsigned char *) lpPerSocketContext->wsabuf.buf;
                msg.len = dwIoSize;
                {

                    lpPerSocketContext->IOOperation = ClientIoRead;
                    dwRecvNumBytes = 0;
                    dwFlags = 0;
                    buffRecv.buf = lpPerSocketContext->Buffer,
                            buffRecv.len = MAX_BUFF_SIZE;


                    nRet = WSARecv(lpPerSocketContext->sessionId, &buffRecv, 1,
                                   &dwRecvNumBytes, &dwFlags, &lpPerSocketContext->Overlapped, NULL);
                    if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                        myprintf("WSARecv() failed: %d\n", WSAGetLastError());
                        self->CloseClient(lpPerSocketContext, FALSE);
                    } else if (self->g_bVerbose) {
                        myprintf("WorkerThread %d: Socket(%d) Send completed (%d bytes), Recv posted\n",
                                 GetCurrentThreadId(), lpPerSocketContext->sessionId, dwIoSize);
                    }

                }
                self->onReadMsg(sessionId, msg);

                if (0) {
                    //PER_SOCKET_CONTEXT* con = (PER_SOCKET_CONTEXT*)malloc(sizeof(PER_SOCKET_CONTEXT));
                    lpPerSocketContext->IOOperation = ClientIoWrite;
                    dwRecvNumBytes = 0;
                    dwFlags = 0;
                    buffRecv.buf = lpPerSocketContext->Buffer,
                            buffRecv.len = MAX_BUFF_SIZE;


                    int nRet = WSASend(lpPerSocketContext->sessionId, &lpPerSocketContext->wsabuf, 1,
                                       &dwSendNumBytes, dwFlags, &(lpPerSocketContext->Overlapped), NULL);

                    if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                        myprintf("WSARecv() failed: %d\n", WSAGetLastError());
                        self->CloseClient(lpPerSocketContext, FALSE);
                    } else if (self->g_bVerbose) {
                        myprintf("WorkerThread %d: Socket(%d) Send completed (%d bytes), Recv posted\n",
                                 GetCurrentThreadId(), lpPerSocketContext->sessionId, dwIoSize);
                    }

                }
                int c = 0;
                if (0) {

                    PER_SOCKET_CONTEXT *con = (PER_SOCKET_CONTEXT *) malloc(sizeof(PER_SOCKET_CONTEXT));
                    if (con) {
                        con->sessionId = lpPerSocketContext->sessionId;
                        con->Overlapped.Internal = 0;
                        con->Overlapped.InternalHigh = 0;
                        con->Overlapped.Offset = 0;
                        con->Overlapped.OffsetHigh = 0;
                        con->Overlapped.hEvent = NULL;
                        con->IOOperation = ClientIoWrite;
                        //con->pIOContextForward = NULL;
                        con->nTotalBytes = 0;
                        con->nSentBytes = 0;
                        con->wsabuf.buf = con->Buffer;
                        con->wsabuf.len = sizeof(con->Buffer);

                        ZeroMemory(con->wsabuf.buf, con->wsabuf.len);
                    }

                    con->sessionId = lpPerSocketContext->sessionId;
                    WSABUF x = {BUFFER_SIZE, (char *) malloc(BUFFER_SIZE)};
                    con->wsabuf = x;
                    con->sessionId = con->sessionId;
                    int sendBytes = 11;
                    con->IOOperation = ClientIoWrite;
                    con->nTotalBytes = sendBytes;
                    con->nSentBytes = 0;
                    con->wsabuf.len = sendBytes;
                    DWORD dwFlags = 0;
                    DWORD dwSendNumBytes = 0;
                    int nRet = 0;
                    nRet = WSASend(con->sessionId, &con->wsabuf, 1,
                                   &dwSendNumBytes, dwFlags, &(con->Overlapped), NULL);

                    if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                        myprintf("WSARecv() failed: %d\n", WSAGetLastError());
                        std::cout << WSAGetLastError() << std::endl;
                        self->CloseClient(lpPerSocketContext, FALSE);
                    } else if (self->g_bVerbose) {
                        myprintf("WorkerThread %d: Socket(%d) Send completed (%d bytes), Recv posted\n",
                                 GetCurrentThreadId(), lpPerSocketContext->sessionId, dwIoSize);
                    }
                }


                break;
            }
            case ClientIoWrite:

                //
                // a write operation has completed, determine if all the data intended to be
                // sent actually was sent.
                //
                lpPerSocketContext->IOOperation = ClientIoWrite;
                lpPerSocketContext->nSentBytes += dwIoSize;

                sessions[sessionId]->sessionId;
                sessions[sessionId]->writeBuffer.erase(dwIoSize);

                self->onWriteBytes(lpPerSocketContext->sessionId, dwIoSize);//TODO x

                if(sessions[sessionId]->writeBuffer.size > 0)
                    this->continueSendMsg(sessionId);
                if (self->sessions[lpPerSocketContext->sessionId]->writeBuffer.size > 0) {

                    //
                    // the previous write operation didn't send all the data,
                    // post another send to complete the operation
                    //
                    /*
                    buffSend.buf = lpPerSocketContext->Buffer + lpPerSocketContext->nSentBytes;
                    buffSend.len = lpPerSocketContext->nTotalBytes - lpPerSocketContext->nSentBytes;
                    nRet = WSASend(lpPerSocketContext->sessionId, &buffSend, 1,
                                   &dwSendNumBytes, dwFlags, &(lpPerSocketContext->Overlapped), NULL);
                    if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                        myprintf("WSASend() failed: %d\n", WSAGetLastError());
                        self->CloseClient(lpPerSocketContext, FALSE);
                    } else if (self->g_bVerbose) {
                        myprintf("WorkerThread %d: Socket(%d) Send partially completed (%d bytes), Recv posted\n",
                                 GetCurrentThreadId(), lpPerSocketContext->sessionId, dwIoSize);
                    }
                     */
                } else {

//                    int sendBytes = std::min<int>(self->sessions[sessionId]->writeBuffer.size, BUFFER_SIZE);
//                    auto &lpPerSocketContext = self->sessions[sessionId]->iocp_write_context;
//                    memcpy(lpPerSocketContext->wsabuf.buf, self->sessions[sessionId]->writeBuffer.data(), sendBytes);//TODO ZEROCPY
//                    lpPerSocketContext->IOOperation = ClientIoWrite;
//                    lpPerSocketContext->nTotalBytes = sendBytes;
//                    lpPerSocketContext->nSentBytes = 0;
//                    lpPerSocketContext->wsabuf.len = sendBytes;
//                    int dwFlags = 0;
//                    DWORD dwSendNumBytes = 0;
//                    int nRet = WSASend(lpPerSocketContext->Socket, &lpPerSocketContext->wsabuf, 1,
//                                       &dwSendNumBytes, dwFlags, &(lpPerSocketContext->Overlapped), NULL);
//                    if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
//                        printf("WSASend() failed: %d\n", WSAGetLastError());
//                        this->CloseClient(lpPerSocketContext, FALSE);
//                        //
//                        // previous write operation completed for this socket, post another recv
//                        //
//                        lpPerSocketContext->IOOperation = ClientIoRead;
//                        dwRecvNumBytes = 0;
//                        dwFlags = 0;
//                        buffRecv.buf = lpPerSocketContext->Buffer,
//                                buffRecv.len = MAX_BUFF_SIZE;
//                        nRet = WSARecv(lpPerSocketContext->Socket, &buffRecv, 1,
//                                       &dwRecvNumBytes, &dwFlags, &lpPerSocketContext->Overlapped, NULL);
//                        if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
//                            myprintf("WSARecv() failed: %d\n", WSAGetLastError());
//                            self->CloseClient(lpPerSocketContext, FALSE);
//                        } else if (self->g_bVerbose) {
//                            myprintf("WorkerThread %d: Socket(%d) Send completed (%d bytes), Recv posted\n",
//                                     GetCurrentThreadId(), lpPerSocketContext->Socket, dwIoSize);
//                        }
//                    }
                }
                break;
            case ClientIoConnect: {
                std::cout << "connect" << std::endl;
                break;
            }

        } //switch
    } //while
    return (0);
}

int Poller::connect(std::string ip, std::string port)
{
    DWORD	dwBytesRet;
    GUID	GuidConnectEx = WSAID_CONNECTEX;
    LPFN_CONNECTEX pfnConnectEx;
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (SOCKET_ERROR == WSAIoctl(g_sdListen, SIO_GET_EXTENSION_FUNCTION_POINTER,
             &GuidConnectEx,
             sizeof(GuidConnectEx),
             &pfnConnectEx,
             sizeof(pfnConnectEx),
             &dwBytesRet,
             NULL,
             NULL))
        std::cout << "ERR:xp" << std::endl;

    PER_SOCKET_CONTEXT* iocp_connect_context = (PER_SOCKET_CONTEXT*)xmalloc(sizeof(PER_SOCKET_CONTEXT));
    memset(iocp_connect_context, 0, sizeof(PER_SOCKET_CONTEXT));
    iocp_connect_context->IOOperation = RWMOD::ClientIoConnect;
    iocp_connect_context->Overlapped.hEvent = NULL;

    sockaddr_in addr;
    memset(&addr, 0, sizeof(sockaddr_in));
    addr.sin_family = AF_INET ;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(std::stoi(port));

    SOCKADDR_IN local;
    local.sin_family = AF_INET;
    local.sin_addr.S_un.S_addr = INADDR_ANY;
    local.sin_port = 0;
    if(SOCKET_ERROR == bind(g_sdListen, (LPSOCKADDR)&local, sizeof(local)))
    {
        printf("绑定套接字失败!\r\n");

        return -1;
    }


    PVOID lpSendBuffer = NULL;
    DWORD dwSendDataLength = 0;
    DWORD dwBytesSent = 0;
    WINBOOL bResult = pfnConnectEx (sock,
                                    (sockaddr *)&addr,  // [in] 对方地址
                                    sizeof (sockaddr_in),               // [in] 对方地址长度
                                    lpSendBuffer ,       // [in] 连接后要发送的内容，这里不用
                                    dwSendDataLength ,   // [in] 发送内容的字节数 ，这里不用
                                    &dwBytesSent ,       // [out] 发送了多少个字节，这里不用
                                    (OVERLAPPED *)iocp_connect_context); // [in] 这东西复杂，下一篇有详解
    if (!bResult )      // 返回值处理
    {
        if ( WSAGetLastError () != ERROR_IO_PENDING )   // 调用失败
        {
            std:: cout << WSAGetLastError () <<std::endl;
            return -1 ;
        }
        else ;// 操作未决（正在进行中 … ）
        {
            // 操作正在进行中
        }
    }
    return 0;
}

int Poller::continueSendMsg(uint64_t sessionId)
{
    int sendBytes = std::min<int>(sessions[sessionId]->writeBuffer.size, BUFFER_SIZE);
    auto &lpPerSocketContext = sessions[sessionId]->iocp_write_context;
    memcpy(lpPerSocketContext->wsabuf.buf, sessions[sessionId]->writeBuffer.data(), sendBytes);//TODO ZEROCPY
    lpPerSocketContext->IOOperation = ClientIoWrite;
    lpPerSocketContext->nTotalBytes = sendBytes;
    lpPerSocketContext->nSentBytes = 0;
    lpPerSocketContext->wsabuf.len = sendBytes;
    int dwFlags = 0;
    DWORD dwSendNumBytes = 0;
    int nRet = WSASend(lpPerSocketContext->sessionId, &lpPerSocketContext->wsabuf, 1,
                       &dwSendNumBytes, dwFlags, &(lpPerSocketContext->Overlapped), NULL);
    if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()))
    {
        printf("WSASend() failed: %d\n", WSAGetLastError());
        this->CloseClient(lpPerSocketContext, FALSE);
        return -1;
    }
    return 0;
}

int Poller::closeSession(uint64_t sessionId)
{
    return 0;
}

int Poller::run(int port) {
    SYSTEM_INFO systemInfo;
    WSADATA wsaData;
    SOCKET sdAccept = INVALID_SOCKET;
    PER_SOCKET_CONTEXT *lpPerSocketContext = NULL;
    DWORD dwRecvNumBytes = 0;
    DWORD dwFlags = 0;
    int nRet = 0;
    //HANDLE iocp = iocps[workerId];

    //INVALID_HANDLE_VALUE
    GetSystemInfo(&systemInfo);
    taskQueue.resize(this->maxWorker);
    //taskQueue.emplace_back(moodycamel::ConcurrentQueue<int>());
    DWORD g_dwThreadCount = systemInfo.dwNumberOfProcessors * 2;

    if ((nRet = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        myprintf("WSAStartup() failed: %d\n", nRet);
        return -1;
    }

    xxx = new std::recursive_mutex;
    g_bEndServer = FALSE;


    iocps.resize(maxWorker);

    for (auto &E:iocps) {
        E = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
        if (E == NULL) {
            myprintf("CreateIoCompletionPort() failed to create I/O completion port: %d\n",
                     GetLastError());
            exit(-16);
        }
    }


    if (!CreateListenSocket())
        exit(-16);
    listenThread = std::thread([&] {


        for (int i = 0; i < maxWorker; i++) {
            workThreads.emplace_back(WorkerThread, this, iocps[i]);
        }

        while (true) {
            sdAccept = WSAAccept(g_sdListen, NULL, NULL, NULL, 0);

            if (sdAccept == SOCKET_ERROR) {
                myprintf("WSAAccept() failed: %d\n", WSAGetLastError());
                exit(-16);
            }
            this->onAccept(sdAccept, Addr());
            int workerId = sdAccept / 4 % maxWorker;
            lpPerSocketContext = UpdateCompletionPort(workerId, sdAccept, ClientIoRead, TRUE);
            if (lpPerSocketContext == NULL)
                exit(-16);


            sessions[sdAccept]->sessionId = sdAccept;
            sessions[sdAccept]->iocp_context = lpPerSocketContext;

            if (g_bEndServer)
                break;
            nRet = WSARecv(sdAccept, &(lpPerSocketContext->wsabuf),
                           1, &dwRecvNumBytes, &dwFlags,
                           &(lpPerSocketContext->Overlapped), NULL);
            if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                myprintf("WSARecv() Failed: %d\n", WSAGetLastError());
                CloseClient(lpPerSocketContext, FALSE);
            }
            //this->connect("211.159.174.69", "6500");

        }


        for (auto &E: workThreads) {
            E.join();
        }


        for (auto &ipcpsE:iocps) {
            if (ipcpsE) {
                for (DWORD i = 0; i < g_dwThreadCount; i++)
                    PostQueuedCompletionStatus(ipcpsE, 0, 0, NULL);
            }
        }

        //TODO CtxtListFree();

        for (auto &ipcpsE:iocps) {
            if (ipcpsE) {
                CloseHandle(ipcpsE);
                ipcpsE = NULL;
            }
        }

        if (g_sdListen != INVALID_SOCKET) {
            closesocket(g_sdListen);
            g_sdListen = INVALID_SOCKET;
        }

        if (sdAccept != INVALID_SOCKET) {
            closesocket(sdAccept);
            sdAccept = INVALID_SOCKET;
        }


    });

    listenThread.join();
    delete xxx;
    WSACleanup();
    //g_bEndServer = true;


    return 0;
}

BOOL Poller::CreateListenSocket(void) {

    int nRet = 0;
    int nZero = 0;
    struct addrinfo hints = {0};
    struct addrinfo *addrlocal = NULL;

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_IP;

    if (getaddrinfo(NULL, g_Port, &hints, &addrlocal) != 0) {
        myprintf("getaddrinfo() failed with error %d\n", WSAGetLastError());
        return (FALSE);
    }

    if (addrlocal == NULL) {
        myprintf("getaddrinfo() failed to resolve/convert the interface\n");
        return (FALSE);
    }

    g_sdListen = WSASocket(addrlocal->ai_family, addrlocal->ai_socktype, addrlocal->ai_protocol,
                           NULL, 0, WSA_FLAG_OVERLAPPED);
    if (g_sdListen == INVALID_SOCKET) {
        myprintf("WSASocket(g_sdListen) failed: %d\n", WSAGetLastError());
        return (FALSE);
    }

    nRet = bind(g_sdListen, addrlocal->ai_addr, (int) addrlocal->ai_addrlen);
    if (nRet == SOCKET_ERROR) {
        myprintf("bind() failed: %d\n", WSAGetLastError());
        return (FALSE);
    }

    nRet = listen(g_sdListen, 5);
    if (nRet == SOCKET_ERROR) {
        myprintf("listen() failed: %d\n", WSAGetLastError());
        return (FALSE);
    }

    //
    // Disable send buffering on the socket.  Setting SO_SNDBUF
    // to 0 causes winsock to stop buffering sends and perform
    // sends directly from our buffers, thereby reducing CPU usage.
    //
    // However, this does prevent the socket from ever filling the
    // send pipeline. This can lead to packets being sent that are
    // not full (i.e. the overhead of the IP and TCP headers is
    // great compared to the amount of data being carried).
    //
    // Disabling the send buffer has less serious repercussions
    // than disabling the receive buffer.
    //
    nZero = 0;
    nRet = setsockopt(g_sdListen, SOL_SOCKET, SO_SNDBUF, (char *) &nZero, sizeof(nZero));
    if (nRet == SOCKET_ERROR) {
        myprintf("setsockopt(SNDBUF) failed: %d\n", WSAGetLastError());
        return (FALSE);
    }

    //
    // Don't disable receive buffering. This will cause poor network
    // performance since if no receive is posted and no receive buffers,
    // the TCP stack will set the window size to zero and the peer will
    // no longer be allowed to send data.
    //

    //
    // Do not set a linger value...especially don't set it to an abortive
    // close. If you set abortive close and there happens to be a bit of
    // data remaining to be transfered (or data that has not been
    // acknowledged by the peer), the PER_SOCKET_CONTEXT will be forcefully reset
    // and will lead to a loss of data (i.e. the peer won't get the last
    // bit of data). This is BAD. If you are worried about malicious
    // clients connecting and then not sending or receiving, the server
    // should maintain a timer on each PER_SOCKET_CONTEXT. If after some point,
    // the server deems a PER_SOCKET_CONTEXT is "stale" it can then set linger
    // to be abortive and close the PER_SOCKET_CONTEXT.
    //

    /*
	LINGER lingerStruct;

	lingerStruct.l_onoff = 1;
	lingerStruct.l_linger = 0;

	nRet = setsockopt(g_sdListen, SOL_SOCKET, SO_LINGER,
					  (char *)&lingerStruct, sizeof(lingerStruct) );
	if( nRet == SOCKET_ERROR ) {
		myprintf("setsockopt(SO_LINGER) failed: %d\n", WSAGetLastError());
		return(FALSE);
	}
    */

    freeaddrinfo(addrlocal);
    return (TRUE);
}


PER_SOCKET_CONTEXT *Poller::UpdateCompletionPort(int workerId, SOCKET sd, RWMOD ClientIo,
                                                 BOOL bAddToList) {

    PER_SOCKET_CONTEXT *lpPerSocketContext = NULL;


    lpPerSocketContext = CtxtAllocate(sd, RWMOD::ClientIoWrite);
    if (lpPerSocketContext == NULL)
        return (NULL);

    lpPerSocketContext = CtxtAllocate(sd, RWMOD::ClientIoRead);
    if (lpPerSocketContext == NULL)
        return (NULL);


    HANDLE iocp = iocps[workerId];
    iocp = CreateIoCompletionPort((HANDLE) sd, iocp, (DWORD_PTR) NULL, 0);
    if (iocp == NULL) {
        myprintf("CreateIoCompletionPort() failed: %d\n", GetLastError());
        //xfree(lpPerSocketContext);
        return (NULL);
    }

    //TODO if (bAddToList) CtxtListAddTo(lpPerSocketContext);

    if (g_bVerbose)
        myprintf("UpdateCompletionPort: Socket(%d) added to IOCP\n", lpPerSocketContext->sessionId);

    return (lpPerSocketContext);
}

VOID Poller::CloseClient(PER_SOCKET_CONTEXT *lpPerSocketContext,
                         BOOL bGraceful) {

    xxx->lock();

    if (lpPerSocketContext) {
        if (g_bVerbose)
            myprintf("CloseClient: Socket(%d) PER_SOCKET_CONTEXT closing (graceful=%s)\n",
                     lpPerSocketContext->sessionId, (bGraceful ? "TRUE" : "FALSE"));
        if (!bGraceful) {

            //
            // force the subsequent closesocket to be abortative.
            //
            LINGER lingerStruct;

            lingerStruct.l_onoff = 1;
            lingerStruct.l_linger = 0;
            setsockopt(lpPerSocketContext->sessionId, SOL_SOCKET, SO_LINGER,
                       (char *) &lingerStruct, sizeof(lingerStruct));
        }
        closesocket(lpPerSocketContext->sessionId);
        lpPerSocketContext->sessionId = INVALID_SOCKET;
        //TODO: remove online list
        lpPerSocketContext = NULL;
    } else {
        myprintf("CloseClient: lpPerSocketContext is NULL\n");
    }
    xxx->unlock();
    return;
}

//
// Allocate a socket context for the new PER_SOCKET_CONTEXT.
//
PER_SOCKET_CONTEXT *Poller::CtxtAllocate(SOCKET sd, RWMOD ClientIO) {

    if (ClientIO == RWMOD::ClientIoRead) {
        PER_SOCKET_CONTEXT *lpPerSocketContext = sessions[sd]->iocp_context;

        xxx->lock();

        if (lpPerSocketContext) {
            lpPerSocketContext->sessionId = sd;
            lpPerSocketContext->Overlapped.Internal = 0;
            lpPerSocketContext->Overlapped.InternalHigh = 0;
            lpPerSocketContext->Overlapped.Offset = 0;
            lpPerSocketContext->Overlapped.OffsetHigh = 0;
            lpPerSocketContext->Overlapped.hEvent = NULL;
            lpPerSocketContext->IOOperation = ClientIO;
            //lpPerSocketContext->pIOContextForward = NULL;
            lpPerSocketContext->nTotalBytes = 0;
            lpPerSocketContext->nSentBytes = 0;
            lpPerSocketContext->wsabuf.buf = lpPerSocketContext->Buffer;
            lpPerSocketContext->wsabuf.len = sizeof(lpPerSocketContext->Buffer);

            ZeroMemory(lpPerSocketContext->wsabuf.buf, lpPerSocketContext->wsabuf.len);
        } else {
            myprintf("HeapAlloc() PER_SOCKET_CONTEXT failed: %d\n", GetLastError());
        }

        xxx->unlock();
        return (lpPerSocketContext);
    } else if (ClientIO == RWMOD::ClientIoWrite) {
        PER_SOCKET_CONTEXT *lpPerSocketContext = sessions[sd]->iocp_write_context;

        xxx->lock();

        if (lpPerSocketContext) {
            lpPerSocketContext->sessionId = sd;
            lpPerSocketContext->Overlapped.Internal = 0;
            lpPerSocketContext->Overlapped.InternalHigh = 0;
            lpPerSocketContext->Overlapped.Offset = 0;
            lpPerSocketContext->Overlapped.OffsetHigh = 0;
            lpPerSocketContext->Overlapped.hEvent = NULL;
            lpPerSocketContext->IOOperation = ClientIO;
            //lpPerSocketContext->pIOContextForward = NULL;
            lpPerSocketContext->nTotalBytes = 0;
            lpPerSocketContext->nSentBytes = 0;
            lpPerSocketContext->wsabuf.buf = lpPerSocketContext->Buffer;
            lpPerSocketContext->wsabuf.len = sizeof(lpPerSocketContext->Buffer);

            ZeroMemory(lpPerSocketContext->wsabuf.buf, lpPerSocketContext->wsabuf.len);
        } else {
            myprintf("HeapAlloc() PER_SOCKET_CONTEXT failed: %d\n", GetLastError());
        }

        xxx->unlock();
        return (lpPerSocketContext);
    }

    return NULL;
}

int myprintf(const char *lpFormat, ...) {

    int nLen = 0;
    int nRet = 0;
    char cBuffer[512];
    va_list arglist;
    HANDLE hOut = NULL;
    HRESULT hRet;

    ZeroMemory(cBuffer, sizeof(cBuffer));

    va_start(arglist, lpFormat);

    nLen = lstrlen(lpFormat);
    hRet = StringCchVPrintf(cBuffer, 512, lpFormat, arglist);

    if (nRet >= nLen || GetLastError() == 0) {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE)
            WriteConsole(hOut, cBuffer, lstrlen(cBuffer), (LPDWORD) &nLen, NULL);
    }

    return nLen;
}

#endif