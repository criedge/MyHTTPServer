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

void http_conn::initmysql_result(connection_pool *connPool)
{
    
}