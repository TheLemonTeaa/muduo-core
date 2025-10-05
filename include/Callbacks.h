#pragma once

#include <memory>
#include <functional>

class Buffer;
class Timestamp;
class TcpConnection;

// 连接建立和断开的回调类型
using ConnectionCallback = std::function<void(const std::shared_ptr<TcpConnection> &)>;
// 读写消息的回调类型
using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection> &, Buffer *buf, Timestamp)>;
// 消息发送完成的回调类型
using WriteCompleteCallback = std::function<void(const std::shared_ptr<TcpConnection> &)>;
// 高水位回调类型
using HighWaterMarkCallback = std::function<void(const std::shared_ptr<TcpConnection> &, size_t)>;
// 连接关闭的回调类型
using CloseCallback = std::function<void(const std::shared_ptr<TcpConnection> &)>;
// TcpConnection的智能指针类型
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;