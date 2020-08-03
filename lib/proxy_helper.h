#ifndef PROXY_HELPER_H
#define PROXY_HELPER_H

#include "pg_proxy.h"
#include "msg_helper.h"
#include <vector>

namespace proxy_h
{
    /* 
        Query the file in the database.

        @param  file_md5    For query
        @param  pg_client   Database handle
        @param  file_name[out] if exist.
        @return 1 if exit, 0 otherwise.
    */
    int Query_file_md5(PGconn* , char [33], pgsql::PgCli*);

    /* 
        Query the file name in the database.

        @param  file_id    For query
        @param  pg_client   Database handle
        @param  file_name[out] if exist, -1 otherwise.
    */
    int Query_file_name(PGconn* , int , pgsql::PgCli*, char *);

    /* 
        Query the block in the database.

        @param  block_md5   For query
        @param  pg_client   Database handle
        @return block_id if exist, 0 otherwise.
    */
    uint Query_block_md5(PGconn* , char [33], pgsql::PgCli*);

    /* 
        Query the block_id of file in the database.

        @param  file_id     For query
        @param  pg_client   Database handle
        @param  query_res[out]   Query result
        @return number of block if exist, 0 otherwise.
    */
    std::vector<Block_Index> Query_file_block(PGconn* , int , pgsql::PgCli*);

    /* 
        Query the block md5 by id in the database.

        @param  block_id   For query
        @param  pg_client   Database handle
        @param  block_md5[out]  Query result
        @return 1 if exist, 0 otherwise.
    */
    bool Query_block_md5_by_id(PGconn* conn_pg, int block_id, pgsql::PgCli* pg_client, char* block_md5);

    /* 
        Query the block sort in t1_file_block.

        @param  file_id   For query
        @param  block_id  For query
        @return block_sort if exist, 0 otherwise.
    */
    uint Query_block_sort(PGconn* conn_pg, int file_id, int block_id, pgsql::PgCli* pg_client);

    int Query_block_id_by_sort(PGconn* conn_pg, int file_id, int block_sort, pgsql::PgCli* pg_client);

    int Insert_t1_file_block(PGconn* conn_pg, uint file_id, uint block_id, uint block_sort, pgsql::PgCli* pg_client);
    int Insert_block_idmd5name(PGconn* conn_pg, uint blockid, char blockmd5[33], char filename[33], pgsql::PgCli* pg_client);
    int Insert_file_idmd5name(PGconn* conn_pg, uint fileid, char filemd5[33], char filename[33], pgsql::PgCli* pg_client);
    uint Query_file_block(PGconn* conn_pg, int file_id, pgsql::PgCli* pg_client, std::vector<Block_Index> &query_res);
    int Get_file_num(PGconn* conn_pg, pgsql::PgCli* pg_client);
}


#endif