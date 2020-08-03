#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libpq-fe.h>

#define COM_LEN 100

bool insert_db  (PGconn *, const char *, const char *);
bool del_db     (PGconn *, const char *, const char *);
bool query_db   (PGconn *, const char *, const char *);
static void show_results(PGresult *);

static void exit_nicely(PGconn *conn)
{
    PQfinish(conn);
    exit(1);
}

int main(int argc, char **argv) 
{
    const char    *conninfo;
    PGconn        *conn;

    conninfo = "dbname=dfs user=casparcai password=1\
                hostaddr=9.134.34.169 port=5432 \
                connect_timeout=5";
    
    /* Connect the database */
    conn = PQconnectdb(conninfo);

    /* Test the connection */
    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to database failed: %s",
                PQerrorMessage(conn));
        exit_nicely(conn);
    }

    /* Test insert */
    if (insert_db(conn, "t1_file_block", "'8','2'"))
    {
        /* do something */
        // exit_nicely(conn);

    }

    /* Test query */
    if (query_db(conn, "t1_file_block", "file_id=8 AND block_id=2"))
    {
        exit_nicely(conn);
    }

    /* Test delete */
    if (del_db(conn, "t1_file_block", "file_id=8 AND block_id=2"))
    {
        exit_nicely(conn);
    }
    
    
    /* End */
    exit_nicely(conn);
}

bool insert_db (PGconn *conn, const char * table, const char *content)
{
    char command[COM_LEN];
    PGresult   *res;

    sprintf(command, "INSERT INTO %s VALUES (%s)", table, content);
    res = PQexec(conn, const_cast<char*>(command));
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "INSERT command failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return 1;
    }
    PQclear(res);
    return 0;
}

bool del_db (PGconn *conn, const char * table, const char *condition)
{
    char command[COM_LEN];
    PGresult   *res;

    sprintf(command, "DELETE FROM %s WHERE %s", table, condition);
    res = PQexec(conn, const_cast<char*>(command));
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "DELETE command failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return 1;
    }
    PQclear(res);
    return 0;
}


bool query_db (PGconn *conn, const char * table, const char *condition)
{
    char command[COM_LEN];
    PGresult   *res;

    sprintf(command, "SELECT * FROM %s WHERE %s", table, condition);
    res = PQexec(conn, const_cast<char*>(command));
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "QUERY command failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return 1;
    }
    show_results(res);
    PQclear(res);
    return 0;
}

static void show_results(PGresult *res)
{
    int res_row = PQntuples(res);
    int res_col = PQnfields(res);
    char *res_char;

    for (int i=0; i<res_row; i++)
    {
        for (int j=0; j<res_col; j++)
        {
            res_char = PQgetvalue(res, i, j);
            printf("%s\t",res_char);
        }
        printf("\n");
    }
}