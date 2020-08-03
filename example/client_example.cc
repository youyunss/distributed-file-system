#include "socket_wrap.h"
#include "msg_helper.h"

using std::string;
int file_size(char* filename);
void get_file(int fp, int confd);
void do_work(int confd);

int main(int argc, char **argv)
{
    struct sockaddr_in serveraddr;
    int confd;

    if (argc != 2)
		perr_exit("usage: tcpcli <IPaddress>");


    // 1、创建一个socket
    confd = socket(AF_INET, SOCK_STREAM, 0);

    // 2、初始化服务器地址
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;//协议为ipv4
    inet_pton(AF_INET, argv[1], &serveraddr.sin_addr.s_addr);//ip地址格式转换
    serveraddr.sin_port = htons(SERVER_PORT);//服务器端口号

    // 3、链接服务器
    Connect(confd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    
    // 4、处理任务
    do_work(confd);

    // 5、关闭socket
    close(confd);
    return 0;
}

/* 发送分块信息表 */
void do_work(int confd)
{
    /* 包头初始化 */
    host_head *send_host_header = new host_head;
    send_host_header->msgtype = ask_for_block;
	send_host_header->fileid = 1;
	send_host_header->blockid = 1;
	string str1 = "1234123412341234";
	strncpy(send_host_header->filemd5, str1.c_str(), 16); 
	string str2 = "4321432143214321";
	strncpy(send_host_header->blockmd5, str2.c_str(), 16);

    /* 分块信息表 */
    Block_Index *send_block_index = new Block_Index[2];
    send_block_index[0].blockid = 3;
	string str3 = "AAAABBBBCCCCDDDD";
	strncpy(send_block_index[0].blockmd5, str3.c_str(), 16); 
	send_block_index[1].blockid = 4;
	string str4 = "EEEEFFFFGGGGHHHH";
	strncpy(send_block_index[1].blockmd5, str4.c_str(), 16); 
	send_host_header->length = 2*sizeof(Block_Index);

    /* 打包 */
    MsgHelper msg_helper;
    string send_msg;
    char send_buf[MAX_DATA_LENGTH+1];
    bzero(send_buf, MAX_DATA_LENGTH + 1);
    msg_head_t *send_msg_header = new msg_head_t;
    msg_helper.set_msg_head(send_msg_header, send_host_header);
	msg_helper.set_msg(send_msg, send_block_index, send_host_header->length);
	msg_helper.pack_msg(send_buf, MAX_DATA_LENGTH, send_msg_header, send_msg);

    /* 发送 */
    int send_len = write(confd, (void*)send_buf, send_host_header->length + sizeof(msg_head_t));
    if (send_len != send_host_header->length + sizeof(msg_head_t))
    {
        perr_exit ("do work: send error");
    }
}

/* 
    Get size of file (in bytes) 
*/
int file_size(char* filename)
{
    struct stat statbuf;
    stat(filename,&statbuf);
    int size=statbuf.st_size;
 
    return size; 
}

/* 
    Read file and send to server 
*/
void get_file(int fp, int confd)
{
	ssize_t		n;
	
	char		sendline[MAXLINE];

	while ((n = read(fp, (void*)sendline, MAXLINE))>0) {
		printf("%ld\n", n);
		write(confd, (void*)sendline, n);
	}
	if (n < 0)
        perr_exit ("get file: read error");

}