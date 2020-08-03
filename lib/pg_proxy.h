#ifndef PG_CLIENT_H_
#define PG_CLIENT_H_

#include <libpq-fe.h>
#include <string>
#include <stdlib.h>
#include <stdio.h>

#define COM_LEN 1024

#define TB_FILE_BLOCK   "t1_file_block"
#define TB_BLOCK        "t2_block"
#define TB_SERVER       "t3_server"
#define TB_FILE         "t4_file"

using std::string; // todo: delete using

namespace pgsql
{
    struct SqlPara 
    {
        string sql_dbname_;//数据库名
        string sql_user_;//用户名
        string sql_psswd_;//密码
        string sql_ip_;//ip
        string sql_port_;//端口，默认5432
        string sql_connect_timeout_;//连接超时
    };

    class PgCli
    {
    public:
        PgCli(SqlPara* para);
        ~PgCli();
        PGconn* Init();
        int InsertDb (PGconn* conn_, const char* table, const char* content);
        int DelDb (PGconn* conn_, const char* table, const char* condition);
        int QueryDb (PGconn* conn_, const char* table, const char* condition);
        int QueryCount (PGconn* conn_, const char * table);
        int QueryBlockCount (PGconn* conn_);
    
    private:
        SqlPara* para_;
        string conn_info_;
        void ShowResults();
        void ExitConn(PGconn* conn_);

    public:
        PGresult* res_;
    };
}

#endif