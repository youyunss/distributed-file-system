CC = g++

LIB_NAME= libdfs.a

INC = -I ../lib
LIBS = -L ../lib -ldfs \
	   -lpthread
CFLAGS = $(INC) -g -O0 -D_REENTRANT -Wall -std=c++11

CLEANFILES = core core.* *.core *.o temp.* *.out typescript* \
		     *.lc *.lh *.bsdi *.sparc *.uw

PROGS = socket_wrap.o pg_proxy.o msg_helper.o proxy_helper.o md5.o

all:	${PROGS}
		ar -rcs ${LIB_NAME} $?

socket_wrap.o: socket_wrap.cc socket_wrap.h
		${CC} ${CFLAGS} -c socket_wrap.cc

pg_proxy.o: pg_proxy.cc pg_proxy.h
		${CC} ${CFLAGS} -c pg_proxy.cc

msg_helper.o: msg_helper.cc msg_helper.h
		${CC} ${CFLAGS} -c msg_helper.cc

proxy_helper.o: proxy_helper.cc proxy_helper.h
		${CC} ${CFLAGS} -c proxy_helper.cc

md5.o : md5.cc md5.h
		${CC} ${CFLAGS} -c md5.cc
clean:
		rm -f ${PROGS} ${CLEANFILES}