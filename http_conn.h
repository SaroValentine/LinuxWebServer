#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include "lock.h"
#include <sys/uio.h>
#include <iostream>

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

    static const int FILENAME_LEN = 200; // 文件名的最大长度
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;

    // HTTP请求方法，但我们仅支持GET
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };

    // 解析客户请求时，主状态机所处的状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0, // 当前正在分析请求行
        CHECK_STATE_HEADER,          // 当前正在分析头部字段
        CHECK_STATE_CONTENT          // 当前正在分析请求体
    };

    // 服务器处理HTTP请求的可能结果
    enum HTTP_CODE
    {
        NO_REQUEST,        // 请求不完整，需要继续读取客户数据
        GET_REQUEST,       // 获得了一个完整的客户请求
        BAD_REQUEST,       // 客户请求有语法错误
        NO_RESOURCE,       // 没有请求的资源
        FORBIDDEN_REQUEST, // 客户对资源没有足够的访问权限
        FILE_REQUEST,      // 客户请求的资源可以正常访问
        INTERNAL_ERROR,    // 服务器内部错误
        CLOSED_CONNECTION  // 客户端已经关闭连接了
    };

    // 从状态机的三种可能状态，即行的读取状态，分别表示
    // 1.读取到一个完整的行 2.行出错 3.行数据尚且不完整
    enum LINE_STATUS
    {
        LINE_OK = 0, // 读取到一个完整的行
        LINE_BAD,    // 行出错
        LINE_OPEN    // 行数据尚且不完整
    };

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

    // 解析HTTP请求
    HTTP_CODE process_read();
    // 填充HTTP应答 
    bool process_write(HTTP_CODE ret); 
    // 解析HTTP请求行
    HTTP_CODE parse_request_line(char *text);
    // 解析HTTP请求头部字段
    HTTP_CODE parse_headers(char *text);
    // 解析HTTP请求体
    HTTP_CODE parse_content(char *text);

    //
    LINE_STATUS parse_line();

    // 获取一行数据
    char *get_line() { return m_read_buf + m_start_line; }; // 返回读缓冲区中已经解析的字符

    HTTP_CODE do_request();

    // 这一组函数被process_write调用以填充HTTP应答。
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_content_type();
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

private:
    char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲区
    int m_read_idx;                    // 标识读缓冲区中已经读入的客户数据的最后一个字节的下一个位置

    int m_checked_idx; // 当前正在分析的字符在读缓冲区中的位置
    int m_start_line;  // 当前正在解析的行的起始位置

    CHECK_STATE m_check_state; // 主状态机当前所处的状态

    void init();

    char *m_url;     // 客户请求的目标文件的文件名
    char *m_version; // HTTP协议版本号，我们仅支持HTTP/1.1
    METHOD m_method; // 请求方法
    char *m_host;    // 主机名
    bool m_linger;   // HTTP请求是否要求保持连接

    int m_content_length;

private:
    char m_real_file[FILENAME_LEN];      // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root是网站根目录
    char m_write_buf[WRITE_BUFFER_SIZE]; // 写缓冲区
    int m_write_idx;                     // 写缓冲区中待发送的字节数
    char *m_file_address;                // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat m_file_stat;             // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct iovec m_iv[2];                // 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量。
    int m_iv_count;

    int bytes_to_send;   // 将要发送的数据的字节数
    int bytes_have_send; // 已经发送的字节数
};

#endif