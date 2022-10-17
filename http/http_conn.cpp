#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your Request has bad syntax or is inherently impossible to satisfy.\n";
const char *error_403_title = "Forbidden";
//form->from
const char *error_403_form = "You do not have permission to get file from this server. \n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving  the request file.\n";

locker m_lock;
map<string, string> users;

int http_conn::m_user_count = 0;
//fd 是0 ~ 65535(上限视情况) 因此 -1 就是 没有分配的意思
int http_conn::m_epollfd = -1;

//输入是连接池类，连接池类有连接池队列
//没有输出，因此这个函数是管理连接池的
void http_conn::initmysql_result(connection_pool *connPool)
{   
    MYSQL *mysql = NULL;
    //注意 connectinoRAII构造函数的第一参数是一个指针的指针
    //因此这里用了寻址
    //回头看MYSQL(即st_mysql)结构体，里边有挺多指针元素的。具体是为什么呢？
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username, passwd数据, 浏览器端输入
    if(mysql_query(mysql, "SELECT username, passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    //MYSQL_RES是mysql库的结构体
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_field(result);

    //从结果集中获取下一行，将对应的用户名和密码存入map类型的变量users中
    while(MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

//对文件描述符设置非阻塞
//这里涉及文件操作fcntl
//注：在函数体内，已经调用fcntl将fd对应的文件的属性修改为new_option代表的属性了
//   那么，返回old_option的原因，是为了备份和恢复到old_option。
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    //O_NONBLOCK 04000 两者进行或操作
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONSHOT

void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    //EPOLLIN 0x002 EPOLLET 1u << 31 EPOLLRDHUP 0x2000
    //注 1U表示这个1是无符号(unsigned)整数
    if( 1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if(one_shot)
        event.events |= EPOLLONESHOT;
    //epoll_ctl 为什么有epollfd和fd这两个fd:
    //epoll同时管理多个socket，当然每个socket有一个文件句柄fd
    //  这个函数里epollfd就像管理员，而fd是被管理的对象中的某一个
    //  这里 对应的事件是ADD 即让epoll开始监听这个fd(对应的socket)的事件
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//从内核事件表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    //在这里，从epoll移除了fd，也就是意味着对应的socket生命周期结束了？
    close(fd);
}

//将事件重置为EPOLLONESHOT
//发现这个event和fd是两回事，回头了解清楚
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if( 1 == TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//关闭连接，关闭一个连接，客户总量减一
void http_conn::close_conn(bool real_close)
{
    if( real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

//初始化连接，外部调用初始化套接字地址
void http_conn::init(   int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                        int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;

    //浏览器重置连接的原因，可能是网站根目录出错 或 http相应格式出错 或 访问的文件内容为空
    //
    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}


//private属性的无参init()函数
//将socket接收的新连接初始化，
//其中 check_state默认为第一个状态：分析请求行状态
void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}


//辅助状态机，用于读取每一行的内容
//返回值为读取行内容的结果，有LINE_OK, LINE_BAD, LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line()
{
    //temp用于接每行数据
    char temp;
    for(; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        //请求行 请求头的每一行，请求头和请求体之间都以CRLF作为终止符号
        //分别是回车(Carriage Return, CR, \r)和换行(Line-Feed, LF, \n)
        if(temp == '\r')
        {
            if((m_checked_idx + 1) == m_read_idx)
            {
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n')
        {
            if(m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}


//循环读取客户数据，直到无数据可读，或对方关闭连接
//非阻塞ET模式下，需要一次性将数据读完
//边缘触发模式检测到文件描述符就绪时，只通知其他程序一次，并假设其已经收到。
bool http_conn::read_once()
{
    //访问越界处理
    if(m_read_idx >=  READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    //LT读数据
    if( 0 == m_TRIGMode)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        m_read_idx += bytes_read;

        if( bytes_read <= 0)
        {
            return false;
        }

        return true;
    }

    //ET读数据
    else
    {
        while(true)
        {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if(bytes_read == -1)
            {
                //ET模式下需要一次性读取所有数据，因此拿到需要继续读的信号时，应该继续执行recv
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;
                }
                return false;
            }
            else if(bytes_read == 0)
            {
                return false;
            }
            m_read_idx += bytes_read;
        }
    }
    return true;
}

//解析请求行
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    //strpbrk 在源字符串s1中找出最先含有搜索字符串(s2)中任意字符的位置并返回
    //若找不到，返回空指针
    //注：设s2为"abc", 则返回s1中第一个从"abc"/"bc"/"c"(优先级分先后)到字符串尾部的字串
    m_url = strpbrk(text, " \t");
    if( !m_url)
    {
        return BAD_REQUEST;
    }

    *m_url++ = '\0';

    char *method = text;
    if(strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else if(strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
    {
        return BAD_REQUEST;
    }
    //strspn和strpbrk相反, 返回str1中第一个不在str2中出现的字符下标
    //即跳过匹配的字符串
    m_url += strspn(m_url, " \t");

    //重复上边的步骤 处理HTTP版本号
    m_version= strpbrk(m_url, " \t");
    if( !m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    // 仅支持HTTP1.1
    if(strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }

    //注意 此处使用的是strncasecmp, 而不是strcasecmp
    if(strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    //注：这里本人认为使用if else 更鲁棒
    if(strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if( !m_url || m_url[0] != '/')
        return BAD_REQUEST;
    if(strlen(m_url) == 1)
        strcat(m_url, "judge.html");

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//传入的text, 是将请求行去掉之后剩下的报文，包含请求头和请求体
//请求头，就是HTTP协议的各种设置参数
//因此，该部分主要逻辑，就是判断请求头的内容完整合法后
//逐个读取该HTTP请求的各种设置参数
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    //判空
    if(text[0] == '\0')
    {
        //判断是GET还是POST请求
        if(m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    //解析connection字段
    else if( strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        //如果是长连接，要设置对应的参数: linger
        if(strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    //解析Content-length字段
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        LOG_INFO("oop!unknown header: %s", text);
    }
    return NO_REQUEST;
}


//判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char * text)
{
    if(m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST请求的最后一部分是输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}


//有限状态机处理请求报文
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while( (m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if(ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if(ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if(ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request()
{
    //不知道这一步的理由。两个是同一级的private变量，
    //区别只在doc_root 是char*, m_real_file是char[]
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    //printf("m_url:%s\n", m_url);
    const char *p = strrchr(m_url, '/');

    if(cgi == 1 && (*(p+1) == '2' || *(p+1) == '3'))
    {
        //读取标志来判断请求的功能是登录还是注册
        char flag = m_url[1];

        //申请200大小的char*来存储URL, 200也是本工程设置的MAX_FILE_NAME大小
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);


        //将用户名和密码提取出来
        //eg: user=1234&passwd=123
        char name[100], password[100];
        int i;
        //从5开始，是因为要跳过前五个字符user=
        for(i = 5; m_string[i] != '&'; ++i)
        {
            name[i-5] = m_string[i];
        }
        name[i-5] = '\0';

        int j = 0;
        for(i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';


    }
}