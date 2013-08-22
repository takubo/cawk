uname_m = $(shell uname -m)

ifeq ($(uname_m), i686)
	ARCH = ARCH_X86
endif

ifeq ($(uname_m), x86_64)
	ARCH = ARCH_X64
endif

CFLAGS = -Wall -fPIC -shared -g -c -O2 \
         -DDYNAMIC \
         -DHAVE_STRING_H -DHAVE_SNPRINTF -DHAVE_STDARG_H -DHAVE_VPRINTF \
         -I${HOME}/gawk-4.0.1 -D${ARCH}

LDFLAGS = -shared

all: cawk.so hello.so
	~/gawk-4.0.1/gawk -f hello.awk

cawk.so: cawk.c makefile
	gcc ${CFLAGS} cawk.c -o cawk.o
	gcc ${LDFLAGS} cawk.o -lffi -o cawk.so

CFLAGS2 = -Wall -fPIC -shared -g -c -O2 
hello.so: hello.c makefile
	gcc ${CFLAGS2} hello.c -o hello.o
	gcc ${LDFLAGS} hello.o -o hello.so
