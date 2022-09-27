#include "webserver.h"

//初始化服务器：
// 1.构造http连接(创建http_conn类的数组, 或者说, 调用数组大小的次数的默认构造函数,创建那个数量的对象)、
// 2.设置根目录
// 3.创建定时器对象
WebServer::WebServer(){
    //
    users = new http_conn[MAX_FD];

    char server_path[200];
    //getcwd: 将当前工作路径放入第一参数BUF,失败返回NULL
    getcwd(server_path, 200);
    //有'\0'哦
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcpy(m_root, root);
    
    users_timer = new client_data[MAX_FD];
}

//释放服务器资源
WebServer::~WebServer(){
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void WebServer::init(int port, string user, string passWord, string databaseName,
                     int log_write, int opt_linger, int trigmode, int sql_num,
                     int thread_num, int close_log, int actor_model)
{
    //抄完发现这些参数从哪里来的啊？没看到啊
    m_port = port;
    m_user = user;
    m_passWord = passWord;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actormodel = actor_model;
}

//很简洁的函数
//注意表达式左值是常量，因为变量m_TRIGMode为空时表达式报错返回未定义值

void WebServer::trig_mode(){
    //LT+LT
    if ( 0 == m_TRIGMode){
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    else if(1 == m_TRIGMode){
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    else if(2 == m_TRIGMode){
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    else if(3 == m_TRIGMode){
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

//初始化日志系统
void WebServer::log_write()
{
    if(0 == m_close_log){
        if(1 == m_log_write)
            //抄到这里报错了，回头实现Log
            Log::get_instance()->init();
    }
}