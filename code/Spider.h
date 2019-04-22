//
// Created by ftp on 4/2/2019.
//

#ifndef MAIN_SPIDER_H
#define MAIN_SPIDER_H

#include "SystemReader.h"

#include <vector>
#include <iostream>
#include <string>
#include <list>
#include <thread>
#include <mutex>
#include <map>
#include "Buffer.h"
#include "NetStruct.h"

#if defined(SELECT_SERVER)
#include "SelectPoller.h"

#elif defined(OS_LINUX)

#include "EpollPoller.h"

#elif defined(OS_DARWIN)

#include "KqueuePoller.h"

#elif defined(OS_WINDOWS)

#include "IOCPPoller.h"

#endif

class Spider : public Poller {
public:
    explicit Spider(int port, int threadsNum = 4);

    Spider &operator=(Spider &&rhs) noexcept;

    virtual ~Spider();

    void run();

    void stop();

    void onAccept(uint64_t sessionId, const Addr &addr) override;

    void connect(const Addr &addr);

    virtual void onConnect(uint64_t sessionId, const Addr &addr);

    void onReadMsg(uint64_t sessionId, const Msg &msg) override;

    void onWriteBytes(uint64_t sessionId, int len) override;

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
};


#endif //MAIN_SPIDER_H
