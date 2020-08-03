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

int main()
{
    int fd=open("../readme.zip.0000001",O_RDONLY);
    char buf[1024*1024];
    read(fd,buf,sizeof(buf));
    int fp_out=open("2.zip",O_RDWR | O_CREAT);
    write(fp_out,buf,BUFSIZ);
    return 0;
}


