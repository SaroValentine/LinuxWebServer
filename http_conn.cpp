#include "http_conn.h"

// 所有的socket上的事件都被注册到同一个epoll内核事件表
int http_conn::m_epollfd = -1;

// 用户数量
int http_conn::m_user_count = 0;

http_conn::http_conn(){}
http_conn::~http_conn(){}

void setnonblocking(int fd)
{
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK; // 设置非阻塞模式
    fcntl(fd, F_SETFL, new_flag);
}

// 添加文件描述符到epoll中
void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    // 设置事件类型为可读事件
    // EPOLLIN: 可读事件
    // EPOLLET: 边缘触发模式
    // EPOLLRDHUP: 对方关闭连接，或者对方关闭了写操作
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;

    // 设置为oneshot模式
    if (one_shot)
    {
        // EPOLLONESHOT: 只监听一次事件
        // 当监听完这次事件后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到epoll队列里
        event.events |= EPOLLONESHOT;
    }
    // 将文件描述符添加到epoll队列中
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    // 设置socket为非阻塞模式
    setnonblocking(fd);
}
// 从epoll中删除文件描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 修改文件描述符,重置socket上的EPOLLONESHOT事件，以确保一次可读，EPOLLIN事件被触发
void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    // 设置事件类型为可读事件
    // EPOLLIN: 可读事件
    // EPOLLET: 边缘触发模式
    // EPOLLRDHUP: 对方关闭连接，或者对方关闭了写操作
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 初始化连接
void http_conn::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    // 端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 将新的连接添加到epoll事件表中
    addfd(m_epollfd, sockfd, true);
    // 用户数量加1
    m_user_count++;
}

// 关闭连接
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        // 将连接从epoll事件表中删除
        removefd(m_epollfd, m_sockfd);
        // 关闭连接
        m_sockfd = -1;
        // 用户数量减1
        m_user_count--;
    }
}

// 读取浏览器发来的全部数据
bool http_conn::read()
{
    std::cout << "一次读完所有的数据" << std::endl;
    return true;
}

// 响应报文写入函数
bool http_conn::write()
{
    std::cout << "一次写完所有的数据" << std::endl;
    return true;
}

// 解析客户端请求
void http_conn::process()
{
    // 接收客户端请求
    std::cout << "接收客户端请求" << std::endl;
    // 解析客户端请求
    std::cout << "解析客户端请求" << std::endl;
}