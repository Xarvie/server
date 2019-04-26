#include "Spider.h"


class Server :public Spider{
public:
    virtual int onReadMsg(uint64_t sessionId, int bytesNum) override
    {
        Session *conn = this->sessions[sessionId];
        Msg msg;
        msg.buff = (unsigned char*)malloc(102400);//conn->readBuffer.buff;
        msg.len = 102400;
        for(int i = 0; i< 100; i++)
        this->sendMsg(sessionId, msg);
        closeConnection(conn);
        return bytesNum;
    }
};
int main()
{
    Server gate;
    gate.create(9876);
    gate.run();

    sleep(23424323);
    return 0;
}