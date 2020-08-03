#include "pg_proxy.h"

using ::pgsql::PgCli;

PgCli::PgCli(SqlPara* para):para_(para),res_(NULL)
{}

PgCli::~PgCli()
{}

PGconn* PgCli::Init()
{
    char command[COM_LEN];
    sprintf(command,
            "dbname=%s user=%s password=%s hostaddr=%s port=%s connect_timeout=%s",
            para_->sql_dbname_.c_str(), para_->sql_user_.c_str(), para_->sql_psswd_.c_str(),
            para_->sql_ip_.c_str(), para_->sql_port_.c_str(), para_->sql_connect_timeout_.c_str());
    PGconn* conn_ = PQconnectdb(const_cast<char*>(command));
    /* Test the connection */
    if (PQstatus(conn_) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to database failed: %s",
                PQerrorMessage(conn_));
        ExitConn(conn_);
    }
    return conn_;
}

int PgCli::InsertDb (PGconn* conn_, const char * table, const char *content)
{
    char command[COM_LEN];
    sprintf(command, "INSERT INTO %s VALUES (%s)", table, content);
    res_ = PQexec(conn_, const_cast<char*>(command));
    if (PQresultStatus(res_) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "INSERT command failed: %s", PQerrorMessage(conn_));
        PQclear(res_);
        return 1;
    }
    PQclear(res_);
    return 0;
}

int PgCli::DelDb (PGconn* conn_, const char * table, const char *condition)
{
    char command[COM_LEN];
    sprintf(command, "DELETE FROM %s WHERE %s", table, condition);
    res_ = PQexec(conn_, const_cast<char*>(command));
    if (PQresultStatus(res_) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "DELETE command failed: %s", PQerrorMessage(conn_));
        PQclear(res_);
        return 1;
    }
    PQclear(res_);
    return 0;
}

int PgCli::QueryDb (PGconn* conn_, const char * table, const char *condition)
{
    char command[COM_LEN];
    sprintf(command, "SELECT * FROM %s WHERE %s", table, condition);
    res_ = PQexec(conn_, const_cast<char*>(command));
    if (PQresultStatus(res_) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "QUERY command failed: %s", PQerrorMessage(conn_));
        PQclear(res_);
        return 1;
    }
    return 0;
}

int PgCli::QueryCount (PGconn* conn_, const char * table)
{
    char command[COM_LEN];
    sprintf(command, "SELECT * FROM %s ORDER BY file_id", table);
    res_ = PQexec(conn_, const_cast<char*>(command));
    if (PQresultStatus(res_) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "QUERY count failed: %s", PQerrorMessage(conn_));
        PQclear(res_);
        return 0;
    }
    else
    {
        int res_row = PQntuples(res_);
        int res_col = PQfnumber(res_, "file_id");
        if (res_row==0)
            return 0;
        else
        {
            char *res_char = PQgetvalue(res_, res_row-1, res_col);
            int file_id = atoi(res_char);
            return file_id;
        }
        
    }
    
    return 0;
}

/* Return block num */
int PgCli::QueryBlockCount (PGconn* conn_)
{
    char command[COM_LEN];
    sprintf(command, "SELECT * FROM %s ORDER BY block_id", TB_BLOCK);
    res_ = PQexec(conn_, command);
    if (PQresultStatus(res_) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "QUERY count failed: %s", PQerrorMessage(conn_));
        PQclear(res_);
        return 0;
    }
    else
    {
        int res_row = PQntuples(res_);
        int res_col = PQfnumber(res_, "block_id");
        if (res_row==0)
            return 0;
        else
        {
            char *res_char = PQgetvalue(res_, res_row-1, res_col);
            int block_id = atoi(res_char);
            return block_id;
        }   
    }
}
void PgCli::ShowResults()
{
    int res_row = PQntuples(res_);
    int res_col = PQnfields(res_);
    char *res_char;
    for (int i=0; i<res_row; i++)
    {
        for (int j=0; j<res_col; j++)
        {
            res_char = PQgetvalue(res_, i, j);
            printf("%s\t",res_char);
        }
        printf("\n");
    }
}

void PgCli::ExitConn(PGconn* conn_)
{
    if (conn_ != nullptr)
    {
        PQfinish(conn_);
    }
    exit(0);
}
