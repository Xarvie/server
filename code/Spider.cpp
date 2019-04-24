//
// Created by ftp on 4/2/2019.
//

#include "Spider.h"
#include "IOCPPoller.h"

#define xmalloc malloc
#define xfree free

#include <atomic>

//std::atomic_int v = 0;

void Spider::create(int port, int threadsNum) {
    sessions.resize(65535);
    for (int i = 0; i < 65535; i++) {
        //sessions[i] = new(xmalloc(sizeof(Session))) Session;
        sessions[i] = new Session;
        sessions[i]->reset();
        sessions[i]->sessionId = (uint64_t)i;

    }
}


Spider &Spider::operator=(Spider &&rhs) noexcept {
    std::cout << "operator=" << std::endl;
    return *this;
}

Spider::~Spider() {

}

int Spider::onReadMsg(uint64_t sessionId, int byteNum) {

    Session *conn = this->sessions[sessionId];

    //conn->readBuffer.
    //if(this->)
    //this->sendMsg(sessionId, msg);
    return byteNum;
}

void Spider::readByte(int byteNum)
{

}

void Spider::onWriteBytes(uint64_t sessionId, int len) {

}

//void Spider::closeSession(uint64_t sessionId) {
////    if(is_active(sessionId))
////        close(sessionId);
//
//    sessions[sessionId]->sessionId = 0;
//}

void Spider::onAccept(uint64_t sessionId, const Addr &addr) {
    sessions[sessionId]->sessionId = sessionId;
}

void Spider::connect(const Addr &addr) {
    char str[256];
}

void Spider::onConnect(uint64_t sessionId, const Addr &addr) {

}

void Spider::run() {
    Poller::run(9876);
}

void Spider::stop() {

}

void Spider::getSessionOption(uint64_t sessionId, int id, int &value) const {

}

void Spider::setSessionOption(uint64_t sessionId, int id, int value) {

}

void Spider::getLoopOption(int id, void *value) const {

}

void Spider::setLoopOption(int id, void *value) {

}

void Spider::checkSessionAlive() {
    for (auto E:sessions) {
        //E->rawSocket.windowsSocket == 0


    }
}
