#include "Spider.h"


class Server :public Poller{
public:
    explicit Server(int port, int maxWorker = 4): Poller(port, maxWorker)
    {

    }
    int onReadMsg(Session &conn, int bytesNum) override
    {
        Msg msg;
        msg.buff = conn.readBuffer.buff;
        msg.len = conn.readBuffer.size;

        this->sendMsg(conn, msg);

        return bytesNum;
    }
};
int main()
{
    Server s(9876, 4);
    s.run();

    return 0;
}