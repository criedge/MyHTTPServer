#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h> //UNIX_STD C 和 C++ 程序设计语言中提供对 POSIX 操作系统 API 的访问功能的头文件的名称
#include <signal.h> //是C标准函数库中的信号处理部分
#include <sys/types.h> //各种各样的数据类型
#include <sys/epoll.h> //epoll
#include <fcntl.h> //FILE_CONTROL 提供unix标准中通用的文件控制函数
#include <sys/socket.h> //SOCKET 编程
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h> //线程库
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
//麻了呀 敲完还是没有找到string 看看下边四个本地头文件里有吗
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

//其他文件需要调用http_conn类
//先声明，之后再实现
class http_conn{
public:
    //读取文件的文件名长度上限
    static const int FILENAME_LEN = 200;
    //读缓存区大小
    static const int READ_BUFFER_SIZE = 2048;
    //写缓存区大小
    static const int WRITE_BUFFER_SIZE = 1024;
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
        PATH
    };
    //主状态机的状态，检查请求报文中元素
    //请求报文POST包含三个部分：请求行，请求头，请求体
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    //HTTP状态码
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    //辅助状态机的状态，文本解析是否成功
    //LINE_OK 读取完整一行
    //LINE_BAD 行出错
    //LINE_OPEN 行数据不完整 继续读取
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn();
    ~http_conn();

public:
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    void close_conn(bool real_close = true);
    //http处理函数
    void process();
    //读取对端浏览器发送的数据
    bool read_once();
    //给对端服务器回应的报文里写入数据
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    //初始化数据库读取线程
    //需要回头实现
    void initmysql_result(connection_pool *connPool);
    
    int timer_flag; // 是否关闭连接
    int improv; //是否正在处理数据

private:
    //重载无参数列表的init
    //有意思的是，http_conn类向外开放了一个有参的init()
    //然后http_conn类有一个私有的init()
    //有参的init函数负责和外界交流初始化需要的数据，且该函数调用私有的init()完成初始化的必要内容
    void init();
    //从m_read_buf读取数据，并处理请求报文
    HTTP_CODE process_read();
    //向m_write_buf写入响应报文数据
    bool process_write(HTTP_CODE ret);
    //主状态机 解析报文中的请求行 的数据
    HTTP_CODE parse_request_line(char *text);
    //主状态机 解析报文中的请求头 的数据
    HTTP_CODE parse_headers(char *text);
    //主状态机 解析报文中的请求体 的数据
    HTTP_CODE parse_content(char *text);
    //生成响应报文
    HTTP_CODE do_request();
    //m_start_line是当前下标，表示已经解析的字符
    //get_line用于将指针向后移动，指向未处理的字符
    char *get_line(){return m_read_buf + m_start_line;};
    //从状态机读取一行，分析是请求报文的哪一部分
    LINE_STATUS parse_line();
    //重要函数，单开一章讲解
    void unmap();
    bool add_responce(const char* format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
    int m_state; //IO 事件类别: 读为0, 写为1

private:
    int m_sockfd;
    sockaddr_in m_address;
    //读缓冲区，用于存储从报文读取到的数据
    char m_read_buf[READ_BUFFER_SIZE];
    //读缓冲下标
    int m_read_idx;
    // m_read_buf中读取的位置m_checked_idx
    int m_checked_idx;
    // m_read_buf已经解析的字符数
    int m_start_line;
    //存储发出的响应报文
    char m_write_buf[WRITE_BUFFER_SIZE];
    //指示buffer中的长度
    int m_write_idx;
    //主状态机的状态
    CHECK_STATE m_check_state;
    //请求报文使用的方法
    METHOD m_method;
    //存储读取文件的名称
    char m_real_file[FILENAME_LEN];
    //请求报文中请求的资源URI
    char *m_url;
    //请求报文的HTTP协议版本
    char *m_version;
    //请求报文的源端地址，即对方客户端的IP地址
    char *m_host;
    int m_content_length;
    bool m_linger;
    //读取服务器上的文件地址
    char *m_file_address;
    struct stat m_file_stat;
    //io向量机制iovec
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi; // 是否启用的POST
    char *m_string; // 存储请求头数据
    //剩余发送字节数
    int bytes_to_send;
    //已发送字节数
    int bytes_have_send;
    char *doc_root;

    //下边这部分是关于用户密码登录的
    map<string, string> m_users; //用户名密码匹配表
    int m_TRIGMode; //触发模式
    int m_close_log; //是否开启日志

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};
#endif