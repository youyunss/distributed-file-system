#include <string>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>
#include "msg_helper.h"

int MsgHelper::pack_msg(char* data, int data_capacity, const msg_head_t *head, const std::string& sendmsg)
{
	int msg_length  = sendmsg.size();
	int data_length = msg_length + MSG_HEAD_LENGTH;
	if (data_capacity < data_length || data_capacity > MAX_DATA_LENGTH || data_length > MAX_DATA_LENGTH)
	{
		return -1;
	}
	memcpy(data, head, MSG_HEAD_LENGTH);
	memcpy((data + MSG_HEAD_LENGTH), sendmsg.c_str(), msg_length);
	return data_length;
}

int MsgHelper::check_msg(const char *data, msg_head_t *head, unsigned check_length)
{
	if (check_length != MSG_HEAD_LENGTH || check_length < 0)
	{
		return -1;
	}
	memcpy(head, data, MSG_HEAD_LENGTH);
	MsgType msgtype = (MsgType)ntohl(head->msgtype);
	if (msgtype != 0 && msgtype != 1 && msgtype != 2 \
		&& msgtype != 3 && msgtype != 4 && msgtype != 5 \
		&& msgtype != 6 && msgtype != 7 && msgtype != 8 \
		&& msgtype != 9  && msgtype != 10)
	{
		return -1;
	}
	return ntohl(head->length);
}

int MsgHelper::unpack_msg(const char* data, int unpack_length, std::string& recvmsg)
{
	if (unpack_length > MAX_DATA_LENGTH || unpack_length < 0)
	{
		return -1;
	}
	if (recvmsg.capacity() < (unsigned)unpack_length)
	{
		recvmsg.resize(unpack_length);
	}
	memcpy(&recvmsg[0], data, unpack_length);
	return 0;
}

int MsgHelper::set_msg_head(msg_head_t *sendhead, const host_head *head)
{
	if (head->msgtype != 0 && head->msgtype != 1 && head->msgtype != 2 \
		&& head->msgtype != 3 && head->msgtype != 4 && head->msgtype != 5 \
		&& head->msgtype != 6 && head->msgtype != 7 && head->msgtype != 8 && head->msgtype != 9)
	{
		return -1;
	}
	sendhead->msgtype = htonl(head->msgtype);
	sendhead->fileid  = htonl(head->fileid);
	sendhead->blockid = htonl(head->blockid);
	sendhead->length  = htonl(head->length);
	memcpy(sendhead->filemd5, head->filemd5,32);
	memcpy(sendhead->blockmd5, head->blockmd5, 32);
	memcpy(sendhead->filename, head->filename, 32);
	return 0;
}

int MsgHelper::get_msg_head(const msg_head_t *recvhead, host_head *head)
{
	MsgType checkmsgtype = (MsgType)ntohl(recvhead->msgtype);
	if (checkmsgtype != 0 && checkmsgtype != 1 && checkmsgtype != 2 \
		&& checkmsgtype != 3 && checkmsgtype != 4 && checkmsgtype != 5 \
		&& checkmsgtype != 6 && checkmsgtype != 7 && checkmsgtype != 8 \
		&& checkmsgtype != 9 && checkmsgtype != 10)
	{
		return -1;
	}
	head->msgtype = (MsgType)ntohl(recvhead->msgtype);
	head->fileid  = ntohl(recvhead->fileid);
	head->blockid = ntohl(recvhead->blockid);
	head->length  = ntohl(recvhead->length);
	memcpy(head->filemd5, recvhead->filemd5, 32);
	memcpy(head->filemd5 + 32, "\0", 1);
	memcpy(head->blockmd5, recvhead->blockmd5, 32);
	memcpy(head->blockmd5 + 32, "\0", 1);
	memcpy(head->filename, recvhead->filename, 32);
	return 0;
}

int MsgHelper::set_msg(std::string& msg, const void *sendmsgsource, int sendmsgsource_length)
{
	if (sendmsgsource_length > MAX_MSG_LENGTH || sendmsgsource_length < 0)
	{
		return -1;
	}
	if (msg.capacity() < (unsigned)sendmsgsource_length)
	{
		msg.resize(sendmsgsource_length);
	}
	memcpy(&msg[0], sendmsgsource, sendmsgsource_length); 
	return 0;
}

int MsgHelper::get_msg(const std::string& msg, void *recvmsgsource, int recvmsgsource_length)
{
	if ((unsigned)recvmsgsource_length < msg.capacity() || recvmsgsource_length > MAX_MSG_LENGTH || recvmsgsource_length < 0)
	{
		return -1;
	}
	int len = msg.size();
	memcpy(recvmsgsource, msg.c_str(), len);
	return len;
}