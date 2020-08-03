#include "proxy_helper.h"
#include "msg_helper.h"
#include "string.h"

namespace proxy_h
{
    /* 
        Query the file in the database.

        @param  file_md5    For query
        @param  pg_client   Database handle
        @return file_id if exist, -1 otherwise.
    */
    int Query_file_md5(PGconn* conn_pg, char file_md5[33], pgsql::PgCli* pg_client)
    {
        char condition[COM_LEN];
        sprintf(condition, "file_md5='%s'", file_md5);
        pg_client->QueryDb(conn_pg, TB_FILE, const_cast<char*>(condition));

        int res_row;
        if ((res_row=PQntuples(pg_client->res_))==0) /* 查询结果为空 */
        {
            PQclear(pg_client->res_);
            return -1;
        } else /* 返回查询结果 */
        {
            /* 查询file_id在哪一列 */
            int res_col = PQfnumber(pg_client->res_, "file_id");
            char *res_char = PQgetvalue(pg_client->res_, 0, res_col);
            PQclear(pg_client->res_);
            int file_id = atoi(res_char);
            return file_id;
        }
        
    }

    /* 
        Query the file name in the database.

        @param  file_id    For query
        @param  pg_client   Database handle
        @param  file_name[out] if exist.
        @return 1 if exit, 0 otherwise.
    */
    int Query_file_name(PGconn* conn_pg, int file_id, pgsql::PgCli* pg_client, char * file_name)
    {
        memset(file_name, 0, 32);
        char condition[COM_LEN];
        sprintf(condition, "file_id='%d'", file_id);
        pg_client->QueryDb(conn_pg, TB_FILE, condition);

        int res_row;
        if ((res_row=PQntuples(pg_client->res_))==0) /* 查询结果为空 */
        {
            PQclear(pg_client->res_);
            return 0;
        } else /* 返回查询结果 */
        {
            /* 查询file_id在哪一列 */
            int res_col = PQfnumber(pg_client->res_, "file_name");
            char *res_char = PQgetvalue(pg_client->res_, 0, res_col);
            strcpy(file_name, res_char);
            PQclear(pg_client->res_);
            return 1;
        }
    }

    /* 
        Query the block in the database.

        @param  block_md5   For query
        @param  pg_client   Database handle
        @return block_id if exist, 0 otherwise.
    */
    uint Query_block_md5(PGconn* conn_pg, char block_md5[33], pgsql::PgCli* pg_client)
    {
        char condition[COM_LEN];
        memset(condition, 0, COM_LEN);
        sprintf(condition, "block_md5='%s'", block_md5);
        pg_client->QueryDb(conn_pg, TB_BLOCK, const_cast<char*>(condition));

        int res_row;
        if ((res_row=PQntuples(pg_client->res_))==0) /* 查询结果为空 */
        {
            PQclear(pg_client->res_);
            return 0;
        } else /* 查询结果不为空 */
        {
            int res_col = PQfnumber(pg_client->res_, "block_id");
            char *res_char = PQgetvalue(pg_client->res_, 0, res_col);
            int block_id = atoi(res_char);
            PQclear(pg_client->res_);
            return block_id;
        }
    }

    /* 
        Query the block_id of file in the database.

        @param  file_id     For query
        @param  pg_client   Database handle
        @param  query_res[out]   Query result
        @return number of block if exist, 0 otherwise.
    */
    uint Query_file_block(PGconn* conn_pg, int file_id, pgsql::PgCli* pg_client, std::vector<Block_Index> &query_res)
    {
        Block_Index block_index_temp;
        char condition[COM_LEN];
        sprintf(condition, "file_id=%d", file_id);
        pg_client->QueryDb(conn_pg, TB_FILE_BLOCK, condition);
        int res_row;
        if ((res_row=PQntuples(pg_client->res_))==0) /* 查询结果为空 */
        {
            PQclear(pg_client->res_);
            return 0;
        } else /* 返回查询结果 */
        {
            int res_col = PQfnumber(pg_client->res_, "block_sort");
            
            /* 循环保存所有 blockid */
            for (int i=0; i<res_row; i++)
            {
                char *res_char = PQgetvalue(pg_client->res_, i, res_col);
                int block_id = atoi(res_char);
                block_index_temp.blockid=block_id;
                char block_md5[33];
                // if (!Query_block_md5_by_id(conn_pg, block_id, pg_client, block_md5))
                //     printf("Block not exist!\n");
                strncpy(block_index_temp.blockmd5, block_md5, 33);
                query_res.push_back(block_index_temp);
            }
            // todo
            PQclear(pg_client->res_);
            return res_row;
        }

    }
    int Insert_t1_file_block(PGconn* conn_pg, uint file_id, uint block_id, uint block_sort, pgsql::PgCli* pg_client)
    {
        char condition[COM_LEN];
        sprintf(condition, "%d, %d, %d", file_id, block_id, block_sort);
        int ret = pg_client->InsertDb(conn_pg, TB_FILE_BLOCK, const_cast<char*>(condition));
    }

    int Insert_block_idmd5name(PGconn* conn_pg, uint blockid, char blockmd5[33], char filename[33], pgsql::PgCli* pg_client)
    {
        char condition[COM_LEN];
        sprintf(condition, "%d, '%s'", blockid, blockmd5);
        int ret = pg_client->InsertDb(conn_pg, TB_BLOCK, const_cast<char*>(condition));
        return ret;
    }

    int Insert_file_idmd5name(PGconn* conn_pg, uint fileid, char filemd5[33], char filename[33], pgsql::PgCli* pg_client)
    {
        char condition[COM_LEN];
        sprintf(condition, "%d, '%s', '%s'", fileid, filemd5, filename);
        int ret = pg_client->InsertDb(conn_pg, TB_FILE, const_cast<char*>(condition));
        return ret;      
    }

    int Get_file_num(PGconn* conn_pg, pgsql::PgCli* pg_client)
    {
        return pg_client->QueryCount(conn_pg, TB_FILE_BLOCK);
    }
    
    /* 
        Query the block md5 by id in the database.

        @param  block_id   For query
        @param  pg_client   Database handle
        @param  block_md5[out]  Query result
        @return 1 if exist, 0 otherwise.
    */
    bool Query_block_md5_by_id(PGconn* conn_pg, int block_id, pgsql::PgCli* pg_client, char* block_md5)
    {
        char condition[COM_LEN];
        sprintf(condition, "block_id=%d", block_id);
        pg_client->QueryDb(conn_pg, TB_BLOCK, condition);
        int res_row;
        if ((res_row=PQntuples(pg_client->res_))==0) /* 查询结果为空 */
        {
            PQclear(pg_client->res_);
            return 0;
        } else /* 查询结果不为空 */
        {
            int res_col = PQfnumber(pg_client->res_, "block_md5");
            char *res_char = PQgetvalue(pg_client->res_, 0, res_col);
            strncpy(block_md5, res_char, 33);
            PQclear(pg_client->res_);
            return 1;
        }
    }

    /* 
        Query the block sort in t1_file_block.

        @param  file_id   For query
        @param  block_id  For query
        @return block_sort if exist, 0 otherwise.
    */
    uint Query_block_sort(PGconn* conn_pg, int file_id, int block_id, pgsql::PgCli* pg_client)
    {
        char condition[COM_LEN];
        sprintf(condition, "file_id=%d and block_id=%d", file_id, block_id);
        pg_client->QueryDb(conn_pg, TB_FILE_BLOCK, condition);
        int res_row;
        if ((res_row=PQntuples(pg_client->res_))==0) /* 查询结果为空 */
        {
            PQclear(pg_client->res_);
            return 0;
        } else /* 查询结果不为空 */
        {
            int res_col = PQfnumber(pg_client->res_, "block_sort");
            char *res_char = PQgetvalue(pg_client->res_, 0, res_col);
            return atoi(res_char);
        }
    }

    int Query_block_id_by_sort(PGconn* conn_pg, int file_id, int block_sort, pgsql::PgCli* pg_client)
    {
        char condition[COM_LEN];
        sprintf(condition, "file_id=%d and block_sort=%d", file_id, block_sort);
        pg_client->QueryDb(conn_pg, TB_FILE_BLOCK, condition);
        int res_row;
        if ((res_row=PQntuples(pg_client->res_))==0) /* 查询结果为空 */
        {
            PQclear(pg_client->res_);
            return 0;
        } else /* 查询结果不为空 */
        {
            int res_col = PQfnumber(pg_client->res_, "block_id");
            char *res_char = PQgetvalue(pg_client->res_, 0, res_col);
            return atoi(res_char);
        }
    }
}
