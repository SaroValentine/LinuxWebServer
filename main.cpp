#include <iostream>
#include "threadpool.h"
#include "lock.h"
#include <exception>
#include <semaphore.h>
#include <list>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "http_conn.h"
#include <sys/epoll.h>
#include <cstdio>

#define MAX_FD 65535           // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 // 最大的事件数

// 添加信号捕捉函数
void addsig(int sig, void(handler)(int))
{
    struct sigaction sa;
    // 注册信号的参数
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

// 添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);
// 从epoll中删除文件描述符
extern void removefd(int epollfd, int fd);
// 修改文件描述符
extern void modfd(int epollfd, int fd, int ev);

int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return -1;
    }

    // 获取端口号
    int port = atoi(argv[1]);

    // 对sigpipe信号进行处理
    addsig(SIGPIPE, SIG_IGN);

    // 初始化线程池
    threadpool<http_conn> *pool = NULL;
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch (...)
    {
        return -1;
    }

    // 创建一个数组用以保存所有的客户端socket文件描述符（客户端信息）
    http_conn *users = new http_conn[MAX_FD];

    // 创建监听socket
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        printf("socket error: %s\n", strerror(errno));
        return -1;
    }

    // 设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定端口
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    int ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    if (ret < 0)
    {
        printf("bind error: %s\n", strerror(errno));
        return -1;
    }

    // 监听端口
    ret = listen(listenfd, 5);
    if (ret < 0)
    {
        printf("listen error: %s\n", strerror(errno));
        return -1;
    }

    // 创建epoll内核事件表，事件数组
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5); // 传递的参数在内核中没有意义，只是一个提示大于零
    if (epollfd < 0)
    {
        printf("epoll_create error: %s\n", strerror(errno));
        return -1;
    }

    // 将监听socket添加到epoll内核事件表中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    // 检测
    while (true)
    {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        // 如果epoll_wait返回值小于0，且错误不是被信号中断，则退出循环
        if (num < 0 && errno != EINTR)
        {
            printf("epoll_wait error: %s\n", strerror(errno));
            break;
        }

        // 循环遍历数组
        for (int i = 0; i < num; i++)
        {
            int sockfd = events[i].data.fd;
            // 如果是监听socket，则处理新的连接
            if (sockfd == listenfd)
            {
                // 有客户端连接进来
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int confd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                if (confd < 0)
                {
                    printf("accept error: %s\n", strerror(errno));
                    continue;
                }

                if (http_conn::m_user_count >= MAX_FD)
                {
                    // 如果连接数超过最大值，则关闭连接
                    // TODO: 给客户端返回错误信息，服务器正忙
                    close(confd);
                    continue;
                }

                // 将新的客户端数据初始化，存放在数组中
                users[confd].init(confd, client_address);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 如果是异常事件，则关闭连接
                users[sockfd].close_conn();
            }
            else if (events[i].events & EPOLLIN)
            {
                // 如果是可读事件，则读取数据
                if (users[sockfd].read())
                {
                    // 如果读取成功，则将任务添加到线程池中
                    pool->append(users + sockfd);
                }
                else
                {
                    // 如果读取失败，则关闭连接
                    users[sockfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                // 如果是可写事件，则写入数据
                if (!users[sockfd].write())
                {
                    // 如果写入失败，则关闭连接
                    users[sockfd].close_conn();
                }
            }
        }

        
    }
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    
    return 0;
}