CC = g++
INC = -I ../lib
LIBS = -L ../lib -ldfs -lpthread -lpq

CFLAGS = -g -O0 -D_REENTRANT -Wall -std=c++11
CLEANFILES = core core.* *.core *.o temp.* *.out typescript* \
		     *.lc *.lh *.bsdi *.sparc *.uw
PROGS = server proxy client
LIB_NAME= libdfs.a

all: ${PROGS}

server: server.cc
	${CC} ${CFLAGS} -o ../bin/server server.cc $(INC) ${LIBS}

client: client.cc
	${CC} ${CFLAGS} -o ../bin/client client.cc $(INC) ${LIBS}

proxy: proxy.cc
	${CC} ${CFLAGS} -o ../bin/proxy proxy.cc $(INC) ${LIBS}



clean:
	rm -f ${PROGS} ${CLEANFILES}