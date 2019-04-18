//
// Created by ftp on 4/2/2019.
//

#ifndef MAIN_SPIDER_H
#define MAIN_SPIDER_H

#include "config.h"

#include <vector>
#include <iostream>
#include <string>
#include <list>
#include <thread>
#include <mutex>
#include <map>
#include "Buffer.h"


#if defined(OS_LINUX)

#include "EpollServer.h"

#elif defined(OS_DARWIN)

#include "KqueueServer.h"

#elif defined(OS_WINDOWS)

#include "IocpServer.h"

#endif

class Spider : public Poller {
public:
    explicit Spider(int port, int threadsNum = 4);

    Spider(Spider &&a) noexcept;

    Spider &operator=(Spider &&rhs) noexcept;

    virtual ~Spider();

    void run();

    void stop();

    virtual void onAccept(uint64_t sessionId, const Addr &addr);

    void connect(const Addr &addr);

    virtual void onConnect(uint64_t sessionId, const Addr &addr);

    void onReadMsg(uint64_t sessionId, const Msg &msg) override;

    void onWriteMsg(uint64_t sessionId, int len);

    void onWriteBytes(uint64_t sessionId, int len) override;

    //void continueSendMsg(uint64_t sessionId);

    //void closeSession(int64_t sessionId);

    void getSessionOption(uint64_t sessionId, int id, int &value) const;

    void setSessionOption(uint64_t sessionId, int id, int value);

    void getLoopOption(int id, void* value) const;

    void setLoopOption(int id, void* value);

protected:
    void checkSessionAlive();

private:


    std::thread initThread;

    bool isRunning = false;
    RawSocket serverSocket;

    std::map<int, void*> spiderOption;
    int num = 4;
};


#endif //MAIN_SPIDER_H
