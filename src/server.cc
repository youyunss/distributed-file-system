#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <memory>
#include "socket_wrap.h"
#include "msg_helper.h"


#define THREAD_NUM 128
#define MAXBUF 2*1024*1024 //2M
#define MSGSIZE 1024*1024
#define HEADSIZE 112 //uint(sizeof(msg_head_t))
using std::string;

struct s_info
{
    struct sockaddr_in cliaddr;
    int connfd;
};

class ServerSocket 
{
private:
    int listenfd;
    int backlog = THREAD_NUM;
    pthread_t tid[THREAD_NUM];
    struct s_info ts[THREAD_NUM];
public:
    ServerSocket(){};
    virtual ~ServerSocket(){};

    void creatSocket();
    void creatSocket(int port);
    void doworkSocket_save(char* backup_ipstr, int& backup_port);
    void doworkSocket_backup();
    void closeSocket();
};

void* save_machine_dowork(void* arg);
void* backup_machine_dowork(void* arg);


void ServerSocket::creatSocket()
{
    // 1、创建socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd<0) {
        perr_exit("socket error");
    }

    // 2、bind()绑定端口号和IP
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));//清零
    servaddr.sin_family = AF_INET;//ipv4协议
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//本地任意ip
    servaddr.sin_port = htons(SERVER_PORT);//端口号
    Bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

    // 3、listen
    listen(listenfd, backlog);//维护已经三次握手和刚刚握手成功的socket队列
    printf("listening in port:%u\n", SERVER_PORT);
};

void ServerSocket::creatSocket(int port)
{
    // 1、创建socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd<0) {
        perr_exit("socket error");
    }

    // 2、bind()绑定端口号和IP
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));//清零
    servaddr.sin_family = AF_INET;//ipv4协议
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//本地任意ip
    servaddr.sin_port = htons(port);//端口号
    Bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

    // 3、listen
    listen(listenfd, backlog);//维护已经三次握手和刚刚握手成功的socket队列
    printf("listening in port:%u\n", port);
};


void ServerSocket::closeSocket()
{
    close(listenfd);
    printf("socket of save_server close, and the program exit !\n");
};

struct save_param {
    struct s_info ts;
    int backup_connfd;
};

void ServerSocket::doworkSocket_save(char* backup_ipstr, int& backup_port)
{
    struct sockaddr_in cliaddr;
    socklen_t cliaddr_len = sizeof(cliaddr);

    // 创建与备份服务器的链接
    struct sockaddr_in backup_addr;
    int backup_connfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&backup_addr, sizeof(backup_addr));
    backup_addr.sin_family = AF_INET;//协议为ipv4
	inet_pton(AF_INET, backup_ipstr, &backup_addr.sin_addr.s_addr);//ip地址格式转换
	backup_addr.sin_port = htons(backup_port);//服务器端口号
    int res = connect(backup_connfd, (struct sockaddr*)&backup_addr, sizeof(backup_addr));
    if(res<0) {
        backup_connfd=-1;
        printf("Warning: connect to backup server failed, will only save!\n");
    }
    else {
        printf("Connect to backup machine success, will save and backup, backup_connfd=%u!\n", backup_connfd);
    }

    int i = 0;
    while(1) {
        // accept阻塞监听代理端链接请求
        int connfd = Accept(listenfd, (struct sockaddr*)&cliaddr, &cliaddr_len);

        // 多线程服务器
        ts[i].cliaddr = cliaddr;
        ts[i].connfd = connfd;
        struct save_param param;
        param.ts = ts[i];
        param.backup_connfd = backup_connfd;
        pthread_create(&tid[i], NULL, save_machine_dowork, (void*)&param);
        i++;
    }
}

void ServerSocket::doworkSocket_backup()
{
    struct sockaddr_in cliaddr;
    socklen_t cliaddr_len = sizeof(cliaddr);
    int i = 0;

    while(1) {
        // accept阻塞监听代理端链接请求
        int connfd = Accept(listenfd, (struct sockaddr*)&cliaddr, &cliaddr_len);

        // 多线程服务器
        ts[i].cliaddr = cliaddr;
        ts[i].connfd = connfd;
        pthread_create(&tid[i], NULL, backup_machine_dowork, (void*)&ts[i]);
        i++;
    }
}

void* save_machine_dowork(void* arg) 
{
    pthread_detach(pthread_self()); //将线程设为分离态

    struct save_param* param = (struct save_param*)arg;
    struct s_info* ts = &param->ts; //代理链接信息
    int backup_connfd = param->backup_connfd; //备份服务器fd

    msg_head_t *recv_msg_header = new msg_head_t;
    host_head *recv_host_header = new host_head;
    MsgHelper msg_helper;
    char recv_buf[MAXBUF];   
    char send_buf[MAXBUF];
    ssize_t n = 0;

    while (1)
    {
        bzero(recv_buf, MAXBUF);
        bzero(send_buf, MAXBUF);

        //读包头
        n = Readn(ts->connfd, recv_buf, sizeof(msg_head_t));
        if (n==-1)
        {
            printf("Connection end!\n");
            break;
        }
        uint recv_msg_len = msg_helper.check_msg(recv_buf, recv_msg_header, n);
        if(recv_msg_len<0) { break; }
        bzero(recv_buf, n);
        recv_host_header->msgtype = no_block;
        msg_helper.get_msg_head(recv_msg_header, recv_host_header);

        const char* filename = recv_host_header->blockmd5;
        string filepath = "./files/" + string(filename);

        if(recv_host_header->msgtype==block_name) 
        {
            //往proxy发文件块
            int filefd = open(filepath.c_str(), O_RDONLY);
            if(filefd < 0) { 
                perr_exit ("do work: open fail!");
            } 
            ssize_t filesize = Readn(filefd, send_buf, MSGSIZE);

            string send_msg;
            recv_host_header->msgtype = block_p2s;
            recv_host_header->length = filesize;
            msg_helper.set_msg_head(recv_msg_header, recv_host_header);
            msg_helper.set_msg(send_msg, send_buf, filesize);
            msg_helper.pack_msg(send_buf, MAX_DATA_LENGTH, recv_msg_header, send_msg);

            n = Writen(ts->connfd, send_buf, HEADSIZE+filesize);
            if(n==HEADSIZE+filesize ) {
                printf("do work: block %s send to proxy success!\n", filename);
            }
            else {
                printf("Send size: %d, data size: %d\n", n, HEADSIZE+filesize);
                break;
            }
        }
        else if (recv_host_header->msgtype==block_p2s)
        {
            //保存文件到本地
            string recv_msg;
            n = Readn(ts->connfd, recv_buf, recv_msg_len);
            if(n!=recv_msg_len) { 
                printf("Warning: read error\n");
                break; 
            }
            int filefd = open(filepath.c_str(), O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
            ftruncate(filefd, 0);
            if(filefd < 0) { break; }
            n = Writen(filefd, recv_buf, recv_msg_len);
            if(n==recv_msg_len) {
                printf("do work: block %s save done!\n", filename);
            }
            else {
                printf("Save size: %d, data size: %d\n", n, recv_msg_len);
                perr_exit ("do work: save error!");
            }

            //发送给备份服务器
            if(backup_connfd!=-1) {
                // printf("start send to backup machine\n");
                string send_msg;
                recv_host_header->msgtype = back_up;
                recv_host_header->length = recv_msg_len;
                msg_helper.set_msg_head(recv_msg_header, recv_host_header);
                msg_helper.set_msg(send_msg, recv_buf, recv_msg_len);
                msg_helper.pack_msg(send_buf, MAX_DATA_LENGTH, recv_msg_header, send_msg);
                n = Writen(backup_connfd, send_buf, HEADSIZE+recv_msg_len);
                if(n==HEADSIZE+recv_msg_len ) {
                    printf("do work: block %s send to backup machine success!\n", filename);
                }
                else {
                    printf("Send size: %d, data size: %d\n, close the connection with backup machine!\n", n, HEADSIZE+recv_msg_len);
                    // Close(backup_connfd);
                    backup_connfd = -1;
                }
            }
        }
        else
        {
            break;
        }
        
    }

    delete recv_msg_header;
    delete recv_host_header;

    return (void*)0;
}

void* backup_machine_dowork(void* arg) 
{
    pthread_detach(pthread_self()); //将线程设为分离态
    struct s_info* ts = (struct s_info*)arg;

    msg_head_t *recv_msg_header = new msg_head_t;
    host_head *recv_host_header = new host_head;
    MsgHelper msg_helper;
    char recv_buf[MAXBUF];   
    char send_buf[MAXBUF];
    ssize_t n = 0;

    while (1)
    {
        bzero(recv_buf, MAXBUF);
        bzero(send_buf, MAXBUF);

        //读包头
        n = Readn(ts->connfd, recv_buf, sizeof(msg_head_t));
        if (n==-1)
        {
            printf("Connection end!\n");
            break;
        }
        uint recv_msg_len = msg_helper.check_msg(recv_buf, recv_msg_header, n);
        if(recv_msg_len<0) { break; }
        bzero(recv_buf, n);
        recv_host_header->msgtype = no_block;
        msg_helper.get_msg_head(recv_msg_header, recv_host_header);

        const char* filename = recv_host_header->blockmd5;
        string filepath = "./files_backup/" + string(filename);

        if(recv_host_header->msgtype==block_name) 
        {
            //往proxy发文件块
            int filefd = open(filepath.c_str(), O_RDONLY);
            if(filefd < 0) { 
                perr_exit ("do work: open fail!");
            } 
            ssize_t filesize = Readn(filefd, send_buf, MSGSIZE);

            string send_msg;
            recv_host_header->msgtype = block_p2s;
            recv_host_header->length = filesize;
            msg_helper.set_msg_head(recv_msg_header, recv_host_header);
            msg_helper.set_msg(send_msg, send_buf, filesize);
            msg_helper.pack_msg(send_buf, MAX_DATA_LENGTH, recv_msg_header, send_msg);

            n = Writen(ts->connfd, send_buf, HEADSIZE+filesize);
            if(n==HEADSIZE+filesize ) {
                printf("do work: block %s send to proxy success!\n", filename);
            }
            else {
                printf("Send size: %d, data size: %d\n", n, HEADSIZE+filesize);
                break;
            }
        }
        else if (recv_host_header->msgtype==block_p2s || recv_host_header->msgtype==back_up)
        {
            //保存文件到本地
            string recv_msg;
            n = Readn(ts->connfd, recv_buf, recv_msg_len);
            if(n!=recv_msg_len) { 
                printf("Warning: read error, close this connection!\n");
                break; 
            }
            int filefd = open(filepath.c_str(), O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
            ftruncate(filefd, 0);
            if(filefd < 0) { break; }
            n = Writen(filefd, recv_buf, recv_msg_len);
            if(n==recv_msg_len) {
                printf("do work: block %s save done!\n", filename);
            }
            else {
                printf("Save size: %d, data size: %d\n", n, recv_msg_len);
                perr_exit ("do work: save error!");
            }
        }
        else
        {
            break;
        }
        
    }

    delete recv_msg_header;
    delete recv_host_header;
    
    //Close(ts->connfd);
    return (void*)0;
}


char backup_ipstr[] = "9.134.42.167";
int backup_port = 1270;

int main(int argc, char** argv)
{   
    if(argc==1) {
        printf("Usage: ./save_server save or ./save_server backup\n");
        exit(0);
    }

    ServerSocket sock;
    if(string(argv[1])=="save") {
        sock.creatSocket(); //绑定到指定的服务器端口
        sock.doworkSocket_save(backup_ipstr, backup_port);
    }
    else if (string(argv[1])=="backup") {
        // sock.creatSocket(); 
        sock.creatSocket(backup_port); //单机测试
        sock.doworkSocket_backup();
    }
    else {
        printf("Usage: ./save_server save or ./save_server backup\n");
        exit(0);
    }

    sock.closeSocket();

    return 0;
}