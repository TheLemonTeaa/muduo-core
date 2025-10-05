#include <string>

#include "TcpServer.h"
#include "Logger.h"

class EchoServer {
    public:
        EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
            : server_(loop, addr, name),
              loop_(loop) {
            // 设置连接回调
            server_.setConnectionCallback(
                std::bind(&EchoServer::onConnection, this, std::placeholders::_1)
            );
            // 设置消息回调
            server_.setMessageCallback(
                std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
            );
        }

        void start() {
            server_.start();
        }
    private:
        // 有新连接或连接断开时的回调
        void onConnection(const std::shared_ptr<TcpConnection> &conn) {
            if (conn->connected()) {
                LOG_INFO("EchoServer - %s connected\n", conn->peerAddress().toIpPort().c_str());
            } else {
                LOG_INFO("EchoServer - %s disconnected\n", conn->peerAddress().toIpPort().c_str());
            }
        }

        // 有读写消息时的回调
        void onMessage(const std::shared_ptr<TcpConnection> &conn, Buffer *buf, Timestamp time) {
            std::string msg = buf->retrieveAllAsString();
            LOG_INFO("EchoServer - received %zd bytes from %s at %s\n",
                     msg.size(), conn->peerAddress().toIpPort().c_str(), time.toString().c_str());
            conn->send(msg); // 回显收到的消息
        }

        TcpServer server_;
        EventLoop *loop_;
};

int main() {
    LOG_INFO("main(): pid = %d\n", getpid());
    EventLoop loop; // 创建事件循环对象
    InetAddress addr(8080); // 监听8080端口
    EchoServer server(&loop, addr, "EchoServer"); // 创建服务器对象
    server.start(); // 启动服务器
    loop.loop(); // 启动事件循环
    return 0;
}