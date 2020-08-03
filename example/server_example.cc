#include "socket_wrap.h"
#include "msg_helper.h"

void save_file(int );

struct s_info
{
    struct sockaddr_in cliaddr;
    int connfd;
};

void* do_work(void* );

int main()
{
    struct sockaddr_in servaddr;
    struct sockaddr_in cliaddr;
    socklen_t cliaddr_len;
    int listenfd;
    int connfd;
    int i = 0;
    pthread_t tid[383];
    struct s_info ts[383];
 
    // 1、创建socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    // 2、bind()绑定端口号和IP
    bzero(&servaddr, sizeof(servaddr));//清零
    servaddr.sin_family =AF_INET;//ipv4协议
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//本地任意ip
    servaddr.sin_port = htons(SERVER_PORT);//端口号
    Bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

    // 3、listen
    listen(listenfd, MAX_CONNECTION);//维护已经三次握手和刚刚握手成功的socket队列

    while (1)
    {
        
    // 4、accept阻塞监听客户端链接请求
        cliaddr_len = sizeof(cliaddr);
        connfd = Accept(listenfd, (struct sockaddr*)&cliaddr, &cliaddr_len);

    // ** 多线程服务器
        ts[i].cliaddr = cliaddr;
        ts[i].connfd = connfd;
        pthread_create(&tid[i], NULL, do_work, (void*)&ts[i]);
        i++;
    }
    close(listenfd);
    return 0;
}

void save_file(int sockfd)
{
	ssize_t		n;
	char		buf[MAXLINE]; // 2 Byte 16 bit * 4096 1KB
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

void* do_work(void* arg)
{
    struct s_info* ts = (struct s_info*)arg;
    pthread_detach(pthread_self());//将线程设为分离态

    /* 读 */
    msg_head_t *recv_msg_header = new msg_head_t;
	host_head *recv_host_header = new host_head;
    MsgHelper msg_helper;
    char recv_buf [MAX_DATA_LENGTH+1];
    bzero(recv_buf, MAX_DATA_LENGTH + 1);

    /* 读包头 */
    uint recv_len = Read_Len(ts->connfd, recv_buf, sizeof(msg_head_t));
    uint recv_msg_len=msg_helper.check_msg(recv_buf, recv_msg_header, recv_len);
    bzero(recv_buf, recv_len);
    msg_helper.get_msg_head(recv_msg_header, recv_host_header);

    /* 读数据 */
    std::string recv_msg;
    Block_Index *recv_block_index = new Block_Index[2];
    recv_len = Read_Len(ts->connfd, recv_buf, recv_msg_len);
    msg_helper.unpack_msg(recv_buf, recv_len, recv_msg);
    msg_helper.get_msg(recv_msg, recv_block_index, recv_len);
    
    /* 输出 */
    for (int i=0; i<2; i++)
    {
        printf("%d,%d,%s\n", i, recv_block_index[i].blockid, recv_block_index[i].blockmd5);
    }
    
    Close(ts->connfd);
    return (void*)0;
}

