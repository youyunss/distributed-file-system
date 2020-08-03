#ifndef _MSG_HELPER_H_
#define _MSG_HELPER_H_

/*
helper使用说明：

发送数据：一个包由包头和string类型的msg组成，主函数里面首先应该定义一个string sendmsg（string类型可以动态设置容量大小）。
		发包时先调用set_msg函数从对应消息类型的数据源（需要先在主函数定义各种消息设置成什么结构体类型的数据，传进来来是应该强转成void *类型）
		获取数据并存储再sendmsg里。最后使用pack_msg函数将头部和msg合pack成一个数据包，最后将其发送出去。

接收数据：一个包由包头和string类型的msg组成，主函数里面首先应该定义一个string recvmsg。
		收包时先调用对应消息类型的unpack_msg函数从缓存中读取出数据包，将具体信息存在recvmsg中，然后返回包头结构体的地址。
		然后再使用get_msg函数将string转换成对应消息类型的结构体中。

定义说明：msg定义成string类型是因为如果定义成char[]，容易浪费长度，如果定义成char *容易覆盖掉其地址后面的内容，导致出错。
		此外string有较多的STL函数使用，不用自己编写一些函数。

*/

#include<string>

#define SERVER_PORT 8702
#define PROXY_PORT 8601
#define BACKUP_PORT 1270

typedef enum MsgType
{
	file_md5=0,
	file_success=1,
	ask_for_block_md5=2,
	block_md5=3,
	ask_for_block=4,
	block_c2p=5,
	ask_for_flie=6,
	block_name=7,
	block_p2s=8,
	no_block=9,
	back_up=10
}MsgType; 

//传输过程中实际添加在消息头部的包头
#pragma  pack(1)//变量以单字节对齐方式int
typedef struct
{
	unsigned msgtype;       //消息类型
	unsigned fileid;        //文件id
	unsigned blockid;       //分块数据序号 
	unsigned length;        //信息长度，数据包长度data_length=length+MSG_HEAD_LENGTH
	char     filemd5[32];   //大文件MD5
	char     blockmd5[32];  //分块数据MD5
	char     filename[32];  //文件名
}msg_head_t;
#pragma  pack()

//这是方便本机对包头进行赋值和读数的一个对应的头部
typedef struct
{
	MsgType  msgtype;      //消息类型
	unsigned fileid;       //文件id
	unsigned blockid;      //分块数据序号 
	unsigned length;       //信息长度
	char     filemd5[36];  //大文件MD5
	char     blockmd5[36]; //分块数据MD5
	char     filename[32]; //文件名
}host_head;

const int MSG_HEAD_LENGTH = sizeof(msg_head_t);
const int HOST_HEAD_LENGTH = sizeof(host_head);
const int MAX_MSG_LENGTH = 1 * 1024 * 1024;                          //1MB
const int MAX_DATA_LENGTH = MAX_MSG_LENGTH + MSG_HEAD_LENGTH;

//长度必须固定,用string不好,这里的数据必须要一个一个字符写进去，才能完整存下。
typedef struct
{
	unsigned blockid;
	char     blockmd5[32];
}Block_Index;

typedef struct 
{
	char blockdata[MAX_MSG_LENGTH];  //blockdata是分块的具体数据
}Block_Data;


class MsgHelper
{
	public:

		static int pack_msg(char* data, int data_capacity, const msg_head_t *head, const std::string& sendmsg);
		//return value:
		//-1, error
		// 0, data_length
		/*使用说明：	data为完整的包数据;sendmsg为message具体内容。data在主函数定义中
 * 				长度设成最大值MAX_MSG_LENGTH。使用时应该先在主函数中，定义好对应的data，sendmsg。
 * 				先调用对应消息的set_msg和set_msg_head函数，然后再调用此函数打包进data。
 * 				data为字符数组，是需要发送的消息,data_capacity为data数组的容量,即MAX_MSG_LENGTH。
 * 		*/

		static int check_msg(const char *data, msg_head_t *head, unsigned check_length);
		//return value:
		//-1, error
		// other, ok, msg_length
		/*使用说明：	read函数可以先只读取头部长度48字节的数据，然后调用check函数去读取包头信息，
 * 				check函数check_length参数值不为48就返回-1。然后返回信息长度msg_length。
 * 				最后再次调用read函数去内核中读取msg_length长度的数据，给unpack函数使用。
 * 		*/

		static int unpack_msg(const char* data, int unpack_length, std::string& recvmsg);
		//return value:
		//-1, error
		//0, ok
		/*使用说明：	data为缓存收到的数据，unpack_length是想要一次性解压的长度，
 * 				是check_msg后再调用read函数的返回值，解压成功会返回0，否则返回-1。
 * 				此函数在再次read函数之后被调用。
		*/

		static int set_msg_head(msg_head_t *sendhead, const host_head *head);
		//return value:
		//-1, error
		//0，ok
		//功能：转换大小端顺序和md5数据类型组建头部
		/*使用说明：	在发送数据前先使用set_msg_head函数打包头部，里面包含类型检查,
 * 				符合类型打包成成功返回0，不符合打包失败返回-1
		*/

		static int get_msg_head(const msg_head_t *recvhead, host_head *head);
		//return value:
		//-1, error
		//0，ok
		//功能：转换大小端顺序和md5数据类型,解析头部
		/*使用说明：	在check_msg后使用get_msg_head函数解析头部之后，里面包含类型检查，符合类型打包成成功返回0，不符合打包失败返回-1
		*/
		
		static int set_msg(std::string& msg, const void *sendmsgsource, int sendmsgsource_length);
		//return value:
		//-1, error
		//0, ok
		//sendmsgsource_length是想要存的长度，为*sendmsgsource的长度即可

		static int get_msg(const std::string& msg, void *recvmsgsource, int recvmsgsource_length);
		//return value:
		//-1, error
		//other, msg_length
		//recvmsgsource_length是容量，为*recvmsgsource的长度，尽量要大一些，保证能够容纳msg数据。
};

#endif