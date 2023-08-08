#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/wait.h>
#include <signal.h>
#include <sys/uio.h>
#include<iostream>

class http_conn
{
private:
    // 该HTTP连接的socket和对方的socket地址
    int m_sockfd;

    // 对方的socket地址
    sockaddr_in m_address;

public:
    // 所有的socket上的事件都被注册到同一个epoll内核事件表中，所以将epoll文件描述符设置为静态的
    static int m_epollfd;

    // 统计用户数量
    static int m_user_count;


    http_conn();
    ~http_conn();

    // 处理客户端请求
    void process();

    // 初始化新接收的连接
    void init(int sockfd, const sockaddr_in &addr);

    // 关闭连接
    void close_conn(bool real_close = true);

    // 读取浏览器端发来的全部数据,非阻塞ET工作模式下，需要一次性将数据读完
    bool read();

    // 响应报文写入函数,非阻塞ET工作模式下，需要一次性将数据写完
    bool write();
};

#endif