#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string>
#include <errno.h>
#include <iostream>
#include <vector>
#include "md5.h"
#include "msg_helper.h"
#include "socket_wrap.h"

using namespace std;

#define READ_DATA_SIZE	1024
#define MD5_SIZE		16
#define MD5_STR_LEN		(MD5_SIZE * 2)

#define BLOCK_SIZE 1048576 // 1 MB  1024 * 1024
#define SERVER_PORT 8765
#define MAXLINE 4096
#define MAXBUF 1*1024*1024+48

#define MAX_BLOCK_NUM 5000   //分块个数上限暂定为5000

void split_file(int fd,vector<string>& vec,char* filename);
void upload(char* filename,int cfd);
void download(int cfd,int fileId);
void merge(int fileId,int num, string &);
int Compute_file_md5(const char *file_path, char *md5_str);
unsigned long file_size(char* filename);
int main(int argc,char** argv)
{
	struct sockaddr_in serveraddr;
	int confd;
	char ipstr[] = "9.134.38.135";
	// char ipstr[] = "127.0.0.1";

	//char buf[MAXLINE];
	//int len;


	// 1、创建一个socket
	confd = socket(AF_INET, SOCK_STREAM, 0);

	// 2、初始化服务器地址
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;//协议为ipv4
	inet_pton(AF_INET, ipstr, &serveraddr.sin_addr.s_addr);//ip地址格式转换
	serveraddr.sin_port = htons(PROXY_PORT);//服务器端口号

	// 3、链接代理服务器
	connect(confd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (confd==-1)
	{
		perr_exit("Connect fail!\n");
	}


	//4、上传命令行参数:upload,filename
	if(string(argv[1])=="upload")
	{
		cout<<"uploading...."<<endl;
		//上传逻辑
		upload(argv[2],confd);

	}
	//下载命令行参数:download,fileId
	else if(string(argv[1])=="download")
	{
		//下载逻辑
		download(confd,atoi(argv[2]));
	}

	// 5、关闭socket
	close(confd);
	return 0;
}

void upload(char* filename,int cfd)
{
	unsigned long filesize=file_size(filename);
	vector<string>allblockmd5;
	allblockmd5.reserve(filesize/(1024*1024)+1);
	//读大文件计算md5,
	int fd=open(filename,O_RDONLY);
	if(fd==-1){
		perror("open file error");
		exit(1);
	}
	char md5_str[MD5_STR_LEN+1];
	if(Compute_file_md5(filename, md5_str)!=0)
	{
		printf("calculate MD5 failed...\n");
		exit(1);
	}
	std::cout<<md5_str<<std::endl;
    
	/*------------------------------------------------------------------*/
	/*------------------------第一次通信传大文件MD5---------------------*/
	/*------------------------------------------------------------------*/
	ssize_t recvlen,sendlen;
	char buf[MAX_DATA_LENGTH+1];
	msg_head_t *sendmsghdr = new msg_head_t;
	msg_head_t *recvmsghdr = new msg_head_t;
	host_head *sendhosthdr = new host_head;
	host_head *recvhosthdr = new host_head;
	MsgHelper sendhelper;
	MsgHelper recvhelper;

	/*打包包头*/
	sendhosthdr->msgtype = file_md5;
	sendhosthdr->length=0;  //没有msg
	strncpy(sendhosthdr->filename,filename,strlen(filename));
	strncpy(sendhosthdr->filemd5, md5_str, 32);
	sendhelper.set_msg_head(sendmsghdr,sendhosthdr);
	bzero(buf,MAX_DATA_LENGTH+1);
	
	/*打包完整包*/
	int data_length=sendhelper.pack_msg(buf,MAX_DATA_LENGTH,sendmsghdr,"");
	sendlen=write(cfd,buf,data_length);  
	if(sendlen!=sizeof(msg_head_t))
	{
		//第一次发送长度必须是包头长度负责出错
		perror("send 1 error");
		exit(1);
	}

	/*------第一次通信接受消息-----*/
	/*读取包头*/
	int fileId;
	bzero(buf,MAX_DATA_LENGTH+1);
	if((recvlen=read(cfd,buf,sizeof(msg_head_t)))<0)
	{
		printf("read 1 error....\n");
		exit(1);
	}
	int recvmsg_length=recvhelper.check_msg(buf, recvmsghdr, recvlen);
    if(recvmsg_length<0)
	{
		printf("msg_head read 1 error....\n");
		exit(1);
	}
	recvhelper.get_msg_head(recvmsghdr, recvhosthdr);
	/*解析包头判断是否需要上传*/
	if(recvhosthdr->msgtype==file_success)
	{
		printf("You have uploaded....\n");
		printf("fileId: %d\n",recvhosthdr->fileid);
		return ;
	}
	//保存改文件id
	fileId=recvhosthdr->fileid;
	delete sendmsghdr;
	delete recvmsghdr;
	delete sendhosthdr;
	delete recvhosthdr;


	/*-----大文件分块并计算md5--------*/
	split_file(fd,allblockmd5,filename);
    

    /*-------------------------------------------------------------------------*/
	/*--------------------------第二次通信上传分块数据表-----------------------*/
    /*-------------------------------------------------------------------------*/
	msg_head_t *sendmsghdr1 = new msg_head_t;
	msg_head_t *recvmsghdr1 = new msg_head_t;
	host_head *sendhosthdr1 = new host_head;
	host_head *recvhosthdr1 = new host_head;
	MsgHelper sendhelper1;
	MsgHelper recvhelper1;

	/*打包包头*/
	sendhosthdr1->msgtype=block_md5;
	sendhosthdr1->fileid=fileId;
	sendhosthdr1->length=allblockmd5.size()*sizeof(Block_Index); 
	strncpy(sendhosthdr1->filename,filename,strlen(filename));
	strncpy(sendhosthdr1->filemd5, md5_str, 32);

	/*数据打包*/
	Block_Index *sendblockindex =new Block_Index[allblockmd5.size()]; 
	for(unsigned int i=0;i<allblockmd5.size();i++)   //规定块ID从1开始
	{
		sendblockindex[i].blockid=i+1;
		strncpy(sendblockindex[i].blockmd5, allblockmd5[i].c_str(), 32);
	}

	/*发送数据*/
	sendhelper1.set_msg_head(sendmsghdr1,sendhosthdr1);
	bzero(buf,MAX_DATA_LENGTH+1);
	int send_len1 = Write(cfd, (void*)sendmsghdr1, sizeof(msg_head_t));
	int send_len2 = Write(cfd, (void*)sendblockindex, sendhosthdr1->length);
	if (send_len1 + send_len2 != sendhosthdr1->length + sizeof(msg_head_t))
	{
		perr_exit("do work: send error");
	}
	
	// sendhelper1.set_msg(sendmsg1,sendblockindex,allblockmd5.size()*sizeof(Block_Index));
    // data_length=sendhelper1.pack_msg(buf,MAX_DATA_LENGTH,sendmsghdr1,sendmsg1);
	// sendlen=Writen(cfd,buf,data_length);
	// if(sendlen==-1)
	// {
	// 	printf("Writen 2 error...\n");
	// 	return ;
	// }

	/*----------第二次通信接受需要上传的分块数据表--------*/
	/*读取包头*/
	bzero(buf,MAX_DATA_LENGTH+1);
	if((recvlen=Readn(cfd,buf,sizeof(msg_head_t)))==-1)
	{
		printf("read head 2 error....\n");
		return ;
	}
	recvmsg_length=recvhelper1.check_msg(buf,recvmsghdr1,recvlen);
	recvhelper1.get_msg_head(recvmsghdr1,recvhosthdr1);

	/*解析包头确定分块信息表的长度*/
    int upload_block_num=(recvhosthdr1->length)/(sizeof(Block_Index));
	Block_Index *recvblockindex =new Block_Index[upload_block_num];

	printf("需要上传的块%d\n",upload_block_num);

	/*读取数据*/
	bzero(buf,recvlen);
	if((recvlen=Readn(cfd,buf,recvmsg_length))==-1)
	{
		printf("read msg2 error....\n");
		return ;
	}
	memcpy(recvblockindex, buf, recvmsg_length);
	// recvhelper1.unpack_msg(buf,recvlen,recvmsg1);
	// recvhelper1.get_msg(recvmsg1,recvblockindex,upload_block_num*sizeof(Block_Index));

	delete sendmsghdr1;
	delete recvmsghdr1;
	delete sendhosthdr1;
	delete recvhosthdr1;


    /*-------------------------------------------------------------------------*/
	/*-------------------------第三次通信上传分块数据---------------------------*/
    /*-------------------------------------------------------------------------*/
	/*解析需要上传的分块信息表*/
	//for(unsigned int i=0;i<upload_block_num;i++)
	char countName[10];
	int name_len = strlen(filename) + 10 + strlen(".");
	char *name = (char *) malloc(name_len);
	for(unsigned int i=0;i<upload_block_num;i++)
	{
		/*block的id从开始*/
		if(recvblockindex[i].blockid!=0)
		{
			/*上传这块数据*/
			msg_head_t *sendmsghdr2 = new msg_head_t;
			host_head *sendhosthdr2 = new host_head;
			MsgHelper sendhelper2;

			/*获取当前分块文件名:根据filename和id拼接文件名*/
			memset(name, 0, name_len);
			sprintf(countName, "%07d", recvblockindex[i].blockid);
			strcpy(name, filename);
			strcat(name, ".");
			strcat(name, countName);
			//	printf("--------%s\n", name);
			int fdi=open(name,O_RDWR);
			char block_buf[MAX_MSG_LENGTH];
			ssize_t blocklen;
			if((blocklen=read(fdi,block_buf,MAX_MSG_LENGTH))==-1)
			{
				perror("read block error");
				return ;
			}

			/*打包头部*/
			sendhosthdr2->msgtype=block_c2p;
			sendhosthdr2->fileid=fileId;
			sendhosthdr2->blockid=recvblockindex[i].blockid;//Id从1开始
			sendhosthdr2->length=blocklen;  //读取每个文件块返回的字节数
			strncpy(sendhosthdr2->blockmd5, allblockmd5[recvblockindex[i].blockid-1].c_str(), 32);
			strncpy(sendhosthdr2->filemd5, md5_str, 32);
			strncpy(sendhosthdr2->filename, filename, 32);

			/*打包数据并发送*/
			bzero(buf,MAX_DATA_LENGTH+1);
			sendhelper2.set_msg_head(sendmsghdr2,sendhosthdr2);
			int send_len1 = Write(cfd, (void*)sendmsghdr2, sizeof(msg_head_t));
			int send_len2 = Write(cfd, (void*)block_buf, sendhosthdr2->length);
			if (send_len1 + send_len2 != sendhosthdr2->length + sizeof(msg_head_t))
			{
				perr_exit("do work: send error");
			}
			// sendhelper2.set_msg(sendmsg2,block_buf,blocklen);  //set_msg中关于block
			// int data_length=sendhelper2.pack_msg(buf,MAX_DATA_LENGTH,sendmsghdr2,sendmsg2);
			// ssize_t sendblocklen=Writen(cfd,buf,data_length);
			// if(sendblocklen==-1)
			// {
			// 	printf("writen block error....\n");
			// 	return ;
			// }
			printf("Upload block: %d\n",recvblockindex[i].blockid);
			//删除当前块
			remove(name);
			delete sendmsghdr2;
			delete sendhosthdr2;
		}
	}
	delete[] sendblockindex;
	delete[] recvblockindex;


	/*第三次通信接收消息*/
	msg_head_t *recvmsghdr2 = new msg_head_t;
	host_head *recvhosthdr2 = new host_head;
	MsgHelper recvhelper2;
	bzero(buf,MAX_MSG_LENGTH+1);

	/*读取包头*/
	recvlen=Readn(cfd,buf,sizeof(msg_head_t));
	if(recvlen==-1)
	{
		printf("read head 3 error...\n");
		return ;
	}
	recvmsg_length=recvhelper2.check_msg(buf, recvmsghdr2, recvlen); 
	recvhelper2.get_msg_head(recvmsghdr2, recvhosthdr2);
	
	/*解析包头*/
	if(recvhosthdr2->msgtype==file_success)
	{
		printf("upload success....\n");
		printf("fileId: %d\n",recvhosthdr2->fileid);
		delete recvmsghdr2;
		delete recvhosthdr2;
		exit(1);
	}
	else
	{
		delete recvmsghdr2;
		delete recvhosthdr2;
		printf("upload failed.....\n");
		exit(0);
	}
}


void download(int cfd,int fileId)
{
    /*-------------------------------------------------------------------------*/
	/*--------------------下载第一次通信,发送文件ID----------------------------*/
    /*-------------------------------------------------------------------------*/
	ssize_t recvlen,sendlen;
	char buf[MAX_DATA_LENGTH+1];
	string recvmsg;
	msg_head_t *sendmsghdr = new msg_head_t;
	msg_head_t *recvmsghdr = new msg_head_t;
	host_head *sendhosthdr = new host_head;
	host_head *recvhosthdr = new host_head;
	MsgHelper sendhelper;
	MsgHelper recvhelper;

	/*打包包头*/
	sendhosthdr->msgtype = ask_for_flie;
	sendhosthdr->fileid=fileId;
	sendhosthdr->length=0;

	/*数据打包并发送*/
	bzero(buf,MAX_DATA_LENGTH+1);
	sendhelper.set_msg_head(sendmsghdr,sendhosthdr);
	int data_length=sendhelper.pack_msg(buf,MAX_DATA_LENGTH,sendmsghdr,"");//msg为空
	sendlen=Writen(cfd,buf,data_length);
	if(sendlen==-1)
	{
		printf("write 1 error....\n");
		return ;
	}

	/*--------第一次通信接收消息:文件块md5表------*/
	/*读取包头*/
	bzero(buf,MAX_DATA_LENGTH+1);
	if((recvlen=Readn(cfd,buf,sizeof(msg_head_t)))==-1)
	{
		printf("read head 1 error,,,\n");
		return ;
	}
	int recvmsg_length=recvhelper.check_msg(buf,recvmsghdr,recvlen);
	recvhelper.get_msg_head(recvmsghdr,recvhosthdr);

	/* 若没查询到文件，返回no_block */
	if (recvhosthdr->msgtype==no_block)
	{
		delete sendmsghdr;
		delete recvmsghdr;
		delete sendhosthdr;
		delete recvhosthdr;
		perr_exit("File not exist in server");
	}
	string filename;
	filename = recvhosthdr->filename;
	/*解析包头获取文件分块信息表的长度*/
	int download_block_num=(recvhosthdr->length)/sizeof(Block_Index);
	std::cout<<"***"<<download_block_num<<std::endl;
	Block_Index *recvblockindex =new Block_Index[download_block_num];

	/*接收分块信息表*/
	bzero(buf,recvlen);
	if((recvlen=Readn(cfd,buf,recvmsg_length))==-1)
	{
		printf("read msg 2 error....\n");
		return ;
	}
	memcpy(recvblockindex, buf, recvlen );
	// recvhelper.unpack_msg(buf,recvlen,recvmsg);
	// recvhelper.get_msg(recvmsg,recvblockindex,download_block_num*sizeof(Block_Index));

	delete sendmsghdr;
	delete recvmsghdr;
	delete sendhosthdr;
	delete recvhosthdr;


    /*-------------------------------------------------------------------------*/
	/*-----------第二次通信查询本地分块文件,上传需要的分块信息表---------------*/
    /*-------------------------------------------------------------------------*/
    
	/*获取需要下载的分块信息表*/
	vector<Block_Index>downloadblock;
	int file_block_num=download_block_num;//该大文件所有分块的个数,其实就是download_block_num
	for(int i=0;i<download_block_num;i++)
	{
		if(recvblockindex[i].blockid!=0)
		{
			/*查询当前块在本地是否存在,若存在加入分块信息表*/
			string blockname="./";
			blockname+=to_string(fileId);
			blockname+="_";
			blockname+=to_string(recvblockindex[i].blockid);
			if((access(blockname.c_str(),F_OK))==-1)
			{
				//本地不存在该块加入下载队列中
				downloadblock.push_back(recvblockindex[i]);
			}
		}
	}

	string sendmsg1;
	msg_head_t *sendmsghdr1 = new msg_head_t;
	host_head *sendhosthdr1 = new host_head;
	MsgHelper sendhelper1;

	/*打包第二次上传的分块信息表*/
	Block_Index *sendblockindex =new Block_Index[downloadblock.size()];
	for(unsigned int i=0;i<downloadblock.size();i++)
	{
		sendblockindex[i]=downloadblock[i];
	}
	printf("需要下载%d\n", downloadblock.size());

	/*打包包头*/
	sendhosthdr1->msgtype=ask_for_block;
	sendhosthdr1->fileid=fileId;
	sendhosthdr1->length=downloadblock.size()*sizeof(Block_Index);

	/*数据打包并上传*/
	bzero(buf,MAX_DATA_LENGTH+1);
	sendhelper1.set_msg_head(sendmsghdr1,sendhosthdr1);
	sendhelper1.set_msg(sendmsg1,sendblockindex,downloadblock.size()*sizeof(Block_Index));
	data_length=sendhelper1.pack_msg(buf,MAX_DATA_LENGTH,sendmsghdr1,sendmsg1);
	sendlen=Writen(cfd,buf,data_length);
	if(sendlen==-1)
	{
		printf("writen 2 error...\n");
		return ;
	}

	delete sendmsghdr1;
	delete sendhosthdr1;

	/*-----------第二次通信接收分块并保存在大文件ID的文件夹下-------*/
	for(unsigned int i=0;i<downloadblock.size();i++)//read次数是需要下载的分块个数
	{
		string recvmsg1;
		msg_head_t *recvmsghdr1 = new msg_head_t;
		host_head *recvhosthdr1 = new host_head;
		MsgHelper recvhelper1;

		/*读取包头*/
		bzero(buf,MAX_DATA_LENGTH+1);
		if((recvlen=Readn(cfd,buf,sizeof(msg_head_t)))==-1)
		{
			printf("read head 2 error....\n");
			return ;
		}
		int recvmsg_length=recvhelper1.check_msg(buf,recvmsghdr1,recvlen);
		recvhelper1.get_msg_head(recvmsghdr1,recvhosthdr1);

		/*读取分块数据*/
		bzero(buf,recvlen);
		if(((recvlen=Readn(cfd,buf,recvmsg_length))==-1))
		{
			printf("read block 2 error....\n");
			return ;
		}

		/*将分块数据保存到本地*/
		string pathname="./";
		pathname+=to_string(fileId);
		pathname+="_";
		pathname+=to_string(recvhosthdr1->blockid);

		int fp = open(pathname.c_str(),O_WRONLY|O_CREAT);
		write(fp,buf,recvlen);

		delete recvmsghdr1;
		delete recvhosthdr1;

	}
	delete[] sendblockindex;
	delete[] recvblockindex;

	//所有通信结束,合并fileID文件夹下的文件块
	merge(fileId,file_block_num, filename);
}

void merge(int fileId,int num, string &filename)
{
	string pathname_f="./";
	pathname_f+=to_string(fileId);
	if(access(pathname_f.c_str(),0) == -1)
	{
		if (mkdir(pathname_f.c_str(), 0777))
		{
			perr_exit("Make dir failed!");
		}
	}
	pathname_f += "/";
	pathname_f += to_string(fileId);
	
	int fp_out = open(pathname_f.c_str(), O_RDWR | O_CREAT);
	printf("---%d---\n",num);
	for(int i=1;i<num+1;i++)
	{
		char buf[MAX_DATA_LENGTH];
		string pathname="./";
		pathname+=to_string(fileId);
		pathname+="_";
		pathname+=to_string(i);
		int fdi;
		if((fdi=open(pathname.c_str(),O_RDWR|O_CREAT))<0)
		{
			perror("merge open error");
			exit(1);
		}
        ssize_t readlen;
		if((readlen=read(fdi,buf,sizeof(buf)))<0)
		{
			perror("merge read error");
			exit(1);
		}
		write(fp_out,buf,readlen);
		//删除当前小块
		remove(pathname.c_str());
	}

}


//文件分块并计算md5
void split_file(int fd,vector<string>&vec,char* fileName)
{
	ssize_t n;
	char buf[1024]; 
	int current_size=0;
	int count=1;

	//char *fileName = "temp";
	char countName[10];
	sprintf(countName, "%07d", count);
	char *name = (char *) malloc(strlen("uploadtmp/")+strlen(fileName) + strlen(countName) + strlen("."));
	strcpy(name, "uploadtmp/");
	strcpy(name, fileName);
	strcat(name, ".");
	strcat(name, countName);
	printf("%s\n", name);
	int fp = open(name,O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

again:
	while((n=read(fd,buf,1024))>0)
	{
		if (current_size + n > BLOCK_SIZE)
		{
			char md5_str[MD5_STR_LEN+1];
			if(Compute_file_md5(name, md5_str)!=0)
			{
				printf("calculate MD5 failed...\n");
				exit(1);
			}
			// cout<<"block md5: "<<md5_str<<endl;;
			vec.push_back(md5_str);

			if(close(fp)==-1)
			  perror("close error");

			/* Create new file */
			sprintf(countName, "%07d", ++count);
			// realloc(name, strlen(fileName) + strlen(countName) + strlen("."));
			memset(name, 0, sizeof(name));
	        strcpy(name, "uploadtmp/");
			strcpy(name, fileName);
			strcat(name, ".");
			strcat(name, countName);
			// printf("%s\n", name);
			fp = open(name,O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
			ftruncate(fp, 0);
			current_size= 0;
		}

		if (write(fp, buf, n)!=n)
		  printf("write error");
		current_size+= n;
	}
	if((count+1)!=vec.size())
	{
		char md5_str[MD5_STR_LEN+1];
		if(Compute_file_md5(name, md5_str)!=0)
		{
			printf("calculate MD5 failed...\n");
			exit(1);
		}
		cout<<"block md5: "<<md5_str<<endl;;
		vec.push_back(md5_str);
	}

	if (n < 0 && errno == EINTR)
	  goto again;
	else if (n < 0)
	  printf("save file: read error");

	if(close(fp)==-1)
	  printf("close error");
}
int Compute_file_md5(const char *file_path, char *md5_str)
{
	int i;
	int fd;
	int ret;
	unsigned char data[READ_DATA_SIZE];
	unsigned char md5_value[MD5_SIZE];
	MD5_CTX md5;

	fd = open(file_path, O_RDONLY);
	if (-1 == fd)
	{
		perror("open_file");
		return -1;
	}

	// init md5
	MD5Init(&md5);

	while (1)
	{
		ret = read(fd, data, READ_DATA_SIZE);
		if (-1 == ret)
		{
			perror("read");
			close(fd);
			return -1;
		}

		MD5Update(&md5, data, ret);

		if (0 == ret || ret < READ_DATA_SIZE)
		{
			break;
		}
	}

	close(fd);

	MD5Final(&md5, md5_value);

	// convert md5 value to md5 string
	for(i = 0; i < MD5_SIZE; i++)
	{
		snprintf(md5_str + i*2, 2+1, "%02x", md5_value[i]);
	}

	return 0;
}
/* 
    Get size of file (in bytes) 
*/
unsigned long file_size(char* filename)
{
    struct stat statbuf;
    stat(filename,&statbuf);
    unsigned long size=statbuf.st_size;
 
    return size; 
}
/*
//将分块文件打包成完整数据文件
void merge(char* filename,int num)
{

	int     n_file;
	sscanf(num, "%d", &n_file );
	printf("%d\n", n_file);

	char    sendline[MAXLINE];
	char*   countName[10];
	int     fp, fp_out;
	int     i, n_read;

	fp_out = open(filename, O_WRONLY | O_CREAT);
	for (i=0; i<n_file; i++)
	{
		sprintf(countName, "%07d", i);
		char *name = (char *) malloc(strlen(argv[1]) + strlen(countName) + strlen("."));
		strcpy(name, filename);
		strcat(name, ".");
		strcat(name, countName);
		printf("%s\n", name);
		fp = open(name, O_RDONLY);
		if (fp==-1)
		  err_sys("open error");

		while ((n_read = read(fp, (void*)sendline, MAXLINE))>0) {
			//printf("%d\n", n);
			if (write(fp_out, (void*)sendline, n_read)!=n_read)
			  err_sys("write error");
		}

		if(close(fp)==-1)
		  err_sys("close error");
	}
}
*/
