#include "socket_wrap.h"
#include "msg_helper.h"
#include "pg_proxy.h"
#include "proxy_helper.h"
#include "consistent_hash.h"
#include <mutex>

struct s_info
{
    struct sockaddr_in cliaddr;
    int conncli_fd;
    int connserv_fd[4];
};


void    save_file(int );    /* 文件保存 */
void*   do_work(void* );    /* 代理服务器事务 */
int init_proxy_socket();   /* 初始化客户端链接套接字 */
int init_server_socket(char ipstr[], int port);/* 初始化服务端链接套接字 */
pgsql::PgCli* init_db_connection(); /* Connect the database */
int get_host_head(int ,host_head* ); /* Get host_head from client */
char BUFFER[MAX_DATA_LENGTH + 1];
uint global_block_num, global_file_num;
mutex mutex_block_id;
mutex mutex_file_id;

int main()
{
    struct sockaddr_in cliaddr;
    socklen_t cliaddr_len;
    int listencli_fd;
    int conncli_fd;
    listencli_fd = init_proxy_socket();
    
    char serverip0[] = "9.134.42.167"; // xu rui
    char serverip1[] = "9.134.40.142"; // zhao yunhao 
    char serverip2[] = "9.134.34.169"; // cai jiahui 
    char serverip_backup[] = "9.134.42.167"; /* backup server */

    /* 初始化block_id, file_id */
    global_block_num = 0;
    global_file_num = 0;
    pgsql::PgCli* pg_client = init_db_connection();
    PGconn* conn_pg = pg_client->Init();
    global_file_num = proxy_h::Get_file_num(conn_pg, pg_client);
    global_block_num = pg_client->QueryBlockCount(conn_pg);
    delete pg_client;

    int i = 0;
    pthread_t tid[383];
    struct s_info ts[383];


    while (1)
    {
        /* accept阻塞监听客户端链接请求 */
        cliaddr_len = sizeof(cliaddr);
        conncli_fd = Accept(listencli_fd, (struct sockaddr*)&cliaddr, &cliaddr_len);

        /* 多线程客户连接 */
        ts[i].cliaddr = cliaddr;
        ts[i].conncli_fd = conncli_fd;
        ts[i].connserv_fd[0] = init_server_socket(serverip0, SERVER_PORT);
        ts[i].connserv_fd[1] = init_server_socket(serverip1, SERVER_PORT);
        // ts[i].connserv_fd[2] = init_server_socket(serverip2, SERVER_PORT);
        ts[i].connserv_fd[3] = init_server_socket(serverip_backup, BACKUP_PORT);
        pthread_create(&tid[i], NULL, do_work, (void*)&ts[i]);
        i++;
    }
    close(listencli_fd);
    return 0;
}

void* do_work(void* arg)
{
    struct s_info* ts = (struct s_info*)arg;
    pthread_detach(pthread_self());//将线程设为分离态

    /* MsgHelper */
    MsgHelper* msg_helper = new MsgHelper;

    /* pg_client Instance */
    pgsql::PgCli* pg_client = init_db_connection();
    PGconn* conn_pg = pg_client->Init();

    int block_nums = 0;

    ConsistentHash* consistent_hash = new ConsistentHash(NODE_NUM);

next_step:
    /* Get msg head */
    host_head* recv_host_header = new host_head;
    int temp_res = get_host_head(ts->conncli_fd, recv_host_header);
    if (temp_res == -1)
        goto end;
    /* MSG TYPE */
    switch (recv_host_header->msgtype)
    {
    /* 
        Query for file md5. 0 
        Send file_success or ask_for_block_md5 to client. 1/2
    */
    case file_md5:
    {
        int file_id = proxy_h::Query_file_md5(conn_pg,recv_host_header->filemd5, pg_client);
        /* 发送给client */
        host_head *send_host_header = new host_head;
        msg_head_t *send_msg_header = new msg_head_t;
        if (file_id != -1)
        {
            /* 包头 */
            send_host_header->msgtype = file_success;
            send_host_header->fileid = file_id;
            send_host_header->length = 0;
            /* 包头转换 */
            msg_helper->set_msg_head(send_msg_header, send_host_header);
            /* 发送 */
            int send_len = Write(ts->conncli_fd, (void*)send_msg_header, sizeof(msg_head_t));
            if (send_len != sizeof(msg_head_t))
            {
                perr_exit ("do work: send error");
            }
        } else
        {
            /* 包头 */
            mutex_file_id.lock();
            int file_id = ++global_file_num;//生成file_id
            mutex_file_id.unlock();

            send_host_header->msgtype = ask_for_block_md5;
            send_host_header->length = 0;
            send_host_header->fileid = file_id;
            /* 包头转换 */
            msg_helper->set_msg_head(send_msg_header, send_host_header);
            /* 发送 */
            int send_len = Write(ts->conncli_fd, (void*)send_msg_header, sizeof(msg_head_t));
            if (send_len != sizeof(msg_head_t))
            {
                perr_exit ("do work: send error");
            }
        }
        delete send_host_header;
        delete send_msg_header;
        delete recv_host_header;

        goto next_step;

    } break;
    case file_success:
        break;
    case ask_for_block_md5:
        break;
    
    /*
        Query block md5. 3
        Then proxy send ask_for_block to client. 4
    */
    case block_md5:
    {
        /* 
            读block信息 
        */
        char recv_buf[MAX_DATA_LENGTH+1];
        int recv_len = Readn(ts->conncli_fd, recv_buf, recv_host_header->length);
        Block_Index *recv_block_index = (Block_Index*) recv_buf;
        int block_count = recv_host_header->length / sizeof(Block_Index);
        /* 
            查询block信息 
        */
        std::vector<Block_Index> ask_for; /* 保存返回结果 */
        for (int block_index = 0; block_index < block_count; block_index++)
        {
            char block_md5_i[33];
            memcpy(block_md5_i, recv_block_index[block_index].blockmd5, 32);
            block_md5_i[32] = 0;
            uint query_res = proxy_h::Query_block_md5(conn_pg, block_md5_i, pg_client);
            if (!query_res) /* 查询不到block */
                ask_for.push_back(recv_block_index[block_index]);
            else /* 查询到block，更新数据库 */
                proxy_h::Insert_t1_file_block(conn_pg, recv_host_header->fileid, query_res, recv_block_index[block_index].blockid, pg_client);
        }
        // TODO: if ask_for.size()==0 send file_success.

        /* ask_for转数组 */
        Block_Index *send_block_index = new Block_Index[ask_for.size()];
        memcpy(send_block_index, &ask_for[0], ask_for.size()*sizeof(Block_Index));
        block_nums = ask_for.size();
        /* 
            返回结果 
        */
        /* 包头 */
        host_head *send_host_header = new host_head;
        send_host_header->msgtype = ask_for_block;
        send_host_header->length = ask_for.size()*sizeof(Block_Index);
        msg_head_t *send_msg_header = new msg_head_t;
        msg_helper->set_msg_head(send_msg_header, send_host_header);
        /* 数据 */
        char send_buf[MAX_DATA_LENGTH+1];
        string send_msg;
        msg_helper->set_msg(send_msg, send_block_index, send_host_header->length);
        msg_helper->pack_msg(send_buf, MAX_DATA_LENGTH, send_msg_header, send_msg);
        /* 发送给客户 */
        int send_len = Write(ts->conncli_fd, (void*)send_buf, send_host_header->length + sizeof(msg_head_t));
        if (send_len != send_host_header->length + sizeof(msg_head_t))
        {
            perr_exit ("do work: send error");
        }
        delete [] send_block_index;
        delete send_host_header;
        delete send_msg_header;
        goto next_step;
    } break;

    /* 
        Client ask for block to download.       4
        Then proxy send block id to server.     7
        Then proxy recv block from server.      8
        Then proxy send block to client.        5
    */
    case ask_for_block:
    {
        /* 
            读block信息 
        */
        char recv_buf[MAX_DATA_LENGTH+1];
        int recv_len = Readn(ts->conncli_fd, recv_buf, recv_host_header->length);
        if(recv_len==-1)
            break;
        Block_Index *recv_block_index = (Block_Index*) recv_buf;
        int block_count = recv_host_header->length / sizeof(Block_Index);
        int file_id = recv_host_header->fileid;
        
        char recv_buf_server[MAX_DATA_LENGTH+1];

        for (int block_index=0; block_index<block_count; block_index++)
        {
            /* 查询block信息所在服务器 */
            // int serverfd = consistent_hash->GetServerIndex(string(recv_block_index[block_index].blockmd5));
            int serverfd = recv_block_index[block_index].blockid % NODE_NUM;
            /* 
                从服务器上下载 
            */
           
            /* 发送包头 block_name */
            host_head *send_host_header = new host_head;
            send_host_header->msgtype = block_name;
            send_host_header->length = 0;
            recv_block_index[block_index].blockid = proxy_h::Query_block_id_by_sort(conn_pg, file_id, recv_block_index[block_index].blockid, pg_client);
            if(!proxy_h::Query_block_md5_by_id(conn_pg, recv_block_index[block_index].blockid, pg_client, send_host_header->blockmd5)) /* 查询md5 */
                perr_exit("Block id not exist");
            msg_head_t *send_msg_header = new msg_head_t;
            msg_helper->set_msg_head(send_msg_header, send_host_header);
ask_server:
            while (true)
            {
                int send_len = Write(ts->connserv_fd[serverfd], (void*)send_msg_header, sizeof(msg_head_t));
                if (send_len==-1)
                {
                    if (serverfd!=3)
                    {
                        printf("Server %d lose connection, use backup server.\n", serverfd);
                        ts->connserv_fd[serverfd] = ts->connserv_fd[3];
                        goto ask_server;                        
                    }
                    else
                    {
                        printf("Backup server lose connection, close connection\n");
                        Close(ts->conncli_fd);
                        goto end;
                    }
                    
                }
                else
                {
                    break;
                }
            }

            /* 
                读服务器发来的数据 
            */
            host_head* recv_host_header_server= new host_head;
            int get_host_len;
            get_host_len = get_host_head(ts->connserv_fd[serverfd],recv_host_header_server);
            if (get_host_len != sizeof(msg_head_t)) 
            {
                if ( serverfd != 3)
                {
                    printf("Server %d lose connection, use backup server.\n", serverfd);
                    ts->connserv_fd[serverfd] = ts->connserv_fd[3];
                    serverfd = 3;
                    goto ask_server;
                }
                else
                {
                    printf("Backup server lose connection, close connection\n");
                    Close(ts->conncli_fd);
                    goto end;
                }
                
            }
            int recv_len;
            if (recv_host_header_server->msgtype==block_p2s)
            {
                if ((recv_len = Readn(ts->connserv_fd[serverfd], (void*)recv_buf_server, recv_host_header_server->length)) !=recv_host_header_server->length )
                {
                    if(serverfd==3)
                    {
                        printf("Backup server lose connection, close connection\n");
                        Close(ts->conncli_fd);
                        goto end;
                    }
                    else
                    {
                        printf("Server %d lose connection, use backup server.\n", serverfd);
                        ts->connserv_fd[serverfd] = ts->connserv_fd[3];
                        serverfd = 3;
                        goto ask_server;
                    }
                    
                }
            }
            delete send_msg_header;
            /* 
                发送给客户 
            */
            string send_msg;
            char send_buf[MAX_DATA_LENGTH+1];
            host_head *send_host_cli_header = new host_head;
            send_host_cli_header->msgtype = block_c2p;
            send_host_cli_header->length = recv_host_header_server->length;
            /* 查询block_sort为block_id */
            send_host_cli_header->blockid = proxy_h::Query_block_sort(conn_pg, file_id, recv_block_index[block_index].blockid, pg_client);
            strncpy(send_host_cli_header->blockmd5, recv_block_index[block_index].blockmd5, 32); 
            msg_head_t *send_msg_cli_header = new msg_head_t;
            msg_helper->set_msg_head(send_msg_cli_header, send_host_cli_header);
            
            int send_len = Write(ts->conncli_fd, (void*)send_msg_cli_header, sizeof(msg_head_t));
            if(send_len==-1)
                break;
            send_len = Write(ts->conncli_fd, (void*)recv_buf_server, send_host_cli_header->length);
            if(send_len==-1)
                break;

            delete send_host_header;
            delete recv_host_header_server;
            delete send_host_cli_header;
            delete send_msg_cli_header;
        }
    } break;

    /* 
        Client send blockmsg to proxy upload.          5
        Then proxy send blockmsg to server upload.     8
    */
    case block_c2p:
    {
        //文件上传
        //接client的包
        char recv_buf[MAX_DATA_LENGTH + 1];
        int recv_len = Readn(ts->conncli_fd, recv_buf, recv_host_header->length);
        if (recv_len!=recv_host_header->length)
        {
            break;
        }          
        printf("Proxy get block %d\n", recv_host_header->blockid);

        //将blockid，blockmd5，filename存入数据库表t2_block //考虑加锁

        mutex_block_id.lock();
        int temp_block_id = ++global_block_num;
        mutex_block_id.unlock();

        proxy_h::Insert_t1_file_block(conn_pg, recv_host_header->fileid, temp_block_id,
                                                    recv_host_header->blockid, pg_client);
        proxy_h::Insert_block_idmd5name(conn_pg, temp_block_id, recv_host_header->blockmd5,
                                                    recv_host_header->filename, pg_client);

        //向server发包
        host_head* send_host_server_header = new host_head;
        //一致性哈希，获取存储服务器id
        // int server_index = consistent_hash->GetServerIndex(string(recv_host_header->blockmd5));
        int server_index = recv_host_header->blockid % NODE_NUM;
        printf("Send to server %d\n",server_index);
        send_host_server_header->msgtype = block_p2s;
        send_host_server_header->blockid = recv_host_header->blockid;
        send_host_server_header->length = recv_host_header->length;        
        strncpy(send_host_server_header->filemd5, recv_host_header->filemd5, 32);
        strncpy(send_host_server_header->blockmd5, recv_host_header->blockmd5, 32);
        msg_head_t* send_msg_server_header = new msg_head_t;
        msg_helper->set_msg_head(send_msg_server_header, send_host_server_header);

        char send_buf[MAX_DATA_LENGTH + 1];
        string send_msg;
        //msg_helper->set_msg(send_msg, BUFFER, send_host_server_header->length);
        //msg_helper->pack_msg(send_buf, MAX_DATA_LENGTH, send_msg_server_header, send_msg);
send_server:
        int send_len1 = Write(ts->connserv_fd[server_index], (void*)send_msg_server_header, sizeof(msg_head_t));
        if (send_len1==-1)
        {
            if(server_index==3)
            {
                printf("Back up server lost connect!");
                break;
            }
            else
            {
                server_index=3;
                goto send_server;
            }
            
        }
        int send_len2 = Write(ts->connserv_fd[server_index], (void*)recv_buf, send_host_server_header->length);
        if (send_len1==-1)
        {
            if(server_index==3)
            {
                printf("Back up server lost connect!");
                break;
            }
            else
            {
                printf("Server %d lost connection, send to back up server!", server_index);
                server_index=3;
                goto send_server;
            }
            
        }
        if (send_len1 + send_len2 != send_host_server_header->length + sizeof(msg_head_t))
        {
            perr_exit("do work: send error");
        }
        printf("Proxy send block %d\n", recv_host_header->blockid);
        delete send_host_server_header;
        delete send_msg_server_header;
        
        block_nums--;
        
        if (block_nums == 0)
        {
            
            int fileid = recv_host_header->fileid;

            //将fileid，filemd5存入数据库t4_file
            int ret = proxy_h::Insert_file_idmd5name(conn_pg, fileid, recv_host_header->filemd5,
                                                        recv_host_header->filename, pg_client);

            //向client发包
            host_head* send_host_cli_header = new host_head;
            send_host_cli_header->msgtype = file_success;
            send_host_cli_header->length = 0;
            send_host_cli_header->fileid = (uint)fileid;
            strncpy(send_host_cli_header->filemd5, recv_host_header->filemd5, 32);

            msg_head_t* send_msg_cli_header = new msg_head_t;
            msg_helper->set_msg_head(send_msg_cli_header, send_host_cli_header);
            int send_len = Write(ts->conncli_fd, (void*)send_msg_cli_header, sizeof(msg_head_t));
            if (send_len != sizeof(msg_head_t))
            {
                perr_exit("do work: send error");
            }
            delete send_host_cli_header;
            delete send_msg_cli_header;
            break;
        }
        goto next_step;
    } break;

    case ask_for_flie:
    {
        /*
            Query blocks
        */
        std::vector<Block_Index> file_blocks;
        uint block_num = 0;
        block_num = proxy_h::Query_file_block(conn_pg, recv_host_header->fileid, pg_client ,file_blocks);

        if (!block_num) /* 查询不到文件 */
        {
            host_head *send_host_header = new host_head;
            send_host_header->msgtype = no_block;
            send_host_header->length = 0;
            msg_head_t *send_msg_header = new msg_head_t;
            msg_helper->set_msg_head(send_msg_header, send_host_header);
            /* 发送 */
            int send_len = Write(ts->conncli_fd, (void*)send_msg_header, sizeof(msg_head_t));
            if (send_len != sizeof(msg_head_t))
            {
                perr_exit ("do work: ask_for_file: send error");
            }
            goto end;
        }
        
        /* 
            发送给client 
        */
        host_head *send_host_header = new host_head;
        send_host_header->msgtype = block_md5;
        send_host_header->length = block_num * sizeof(Block_Index);
        /* 文件名 */
        proxy_h::Query_file_name(conn_pg, recv_host_header->fileid, pg_client, send_host_header->filename);
        msg_head_t *send_msg_header = new msg_head_t;
        msg_helper->set_msg_head(send_msg_header, send_host_header);
        /* vector转数组 */
        Block_Index *send_block_index = new Block_Index[file_blocks.size()];
        memcpy(send_block_index, &file_blocks[0], file_blocks.size()*sizeof(Block_Index));
        /* 数据 */
        char send_buf[MAX_DATA_LENGTH+1];
        string send_msg;
        msg_helper->set_msg(send_msg, send_block_index, send_host_header->length);
        msg_helper->pack_msg(send_buf, MAX_DATA_LENGTH, send_msg_header, send_msg);
        /* 发送 */
        int send_len = Write(ts->conncli_fd, (void*)send_buf, sizeof(msg_head_t) + send_host_header->length);
        if (send_len != sizeof(msg_head_t) + send_host_header->length)
        {
            perr_exit ("do work: ask_for_file: send error");
        }
        
        goto next_step;
    } break;
    /* 
        no use
        Server send blockmsg to proxy download.        8
        Then proxy send blockmsg to server download.     5
    */

    case no_block:
        break;
    
    /* Wrong massage */
    default:
        break;
    }
end:
    delete msg_helper;
    delete pg_client;
    delete consistent_hash;

    return (void*)0;
}

/* 初始化客户端链接套接字 */
int init_proxy_socket()
{
    struct sockaddr_in proxyaddr;
    int listenfd;
 
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&proxyaddr, sizeof(proxyaddr));
    proxyaddr.sin_family =AF_INET;
    proxyaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    proxyaddr.sin_port = htons(PROXY_PORT);//端口号
    Bind(listenfd, (struct sockaddr*)&proxyaddr, sizeof(proxyaddr));

    Listen(listenfd, MAX_CONNECTION);
    
    return listenfd;
}

/* 初始化服务端链接套接字 */
int init_server_socket(char ipstr[], int port)
{
    struct sockaddr_in serveraddr;
    int connfd;

    connfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, ipstr, &serveraddr.sin_addr.s_addr);//服务器IP
    serveraddr.sin_port = htons(port);//端口号
    Connect(connfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

    return connfd;
}

/* Connect the database */
pgsql::PgCli* init_db_connection()
{
    pgsql::SqlPara *sql_para = new pgsql::SqlPara();
    sql_para->sql_dbname_ = "dfs";
    sql_para->sql_ip_ =  "9.134.34.169";
    sql_para->sql_port_ = "5432";
    sql_para->sql_psswd_ = "1";
    sql_para->sql_user_ = "casparcai";
    sql_para->sql_connect_timeout_ = "5";
    pgsql::PgCli *pg_client = new pgsql::PgCli(sql_para);
    return pg_client;
}

int get_host_head(int connfd, host_head *recv_host_header)
{
    MsgHelper msg_helper;
    msg_head_t *recv_msg_header = new msg_head_t;

    char recv_buf [MAX_DATA_LENGTH+1];
    bzero(recv_buf, MAX_DATA_LENGTH + 1);

    /* 读包头 */
    int recv_len = Readn(connfd, recv_buf, sizeof(msg_head_t));
    if(recv_len == -1)
        return -1;
    uint recv_msg_len=msg_helper.check_msg(recv_buf, recv_msg_header, recv_len);
    bzero(recv_buf, recv_len);
    msg_helper.get_msg_head(recv_msg_header, recv_host_header);
    delete recv_msg_header;
    return recv_len;
}

void save_file(int sockfd)
{
    ssize_t		n;
    char		buf[MAXLINE];
    long 		current_size= 0;

    const char *fileName = "file.temp";
    int fp = open(fileName,O_WRONLY|O_CREAT);

    if (fp==-1)
        perr_exit("open error");
again:
    while ( (n = read(sockfd, buf, MAXLINE)) > 0)
    {
        Writen(fp, buf, n);
        current_size+= n;
    }

    if (n < 0 && errno == EINTR)
        goto again;
    else if (n < 0)
        perr_exit("save file: read error");

    if(close(fp)==-1)
        perr_exit("close error");

}