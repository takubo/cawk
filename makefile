CFLAGS = -Wall -fPIC -shared -g -c -O2 -DHAVE_STRING_H -DHAVE_SNPRINTF -DHAVE_STDARG_H -DHAVE_VPRINTF -DDYNAMIC -I/home/takubo/gawk-4.0.1
LDFLAGS = -shared
KAO = cawk

all: ${KAO}.so hello.so
	~/gawk-4.0.1/gawk -f hello.awk

${KAO}.so: ${KAO}.c makefile
	gcc ${CFLAGS} ${KAO}.c -o ${KAO}.o
	gcc ${LDFLAGS} ${KAO}.o -lffi -o ${KAO}.so

CFLAGS2 = -Wall -fPIC -shared -g -c -O2 
hello.so: hello.c makefile
	gcc ${CFLAGS2} hello.c -o hello.o
	gcc ${LDFLAGS} hello.o -o hello.so
