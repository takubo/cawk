uname_m = $(shell uname -m)

# Linux format
ifeq (${uname_m}, i686)
	ARCH = ARCH_X86
endif

# Linux format
ifeq (${uname_m}, x86_64)
	ARCH = ARCH_X64
endif

# FreeBSD format
ifeq (${uname_m}, amd64)
	ARCH = ARCH_X64
endif

CFLAGS = -Wall -fPIC -shared -g -c -O2 \
         -DDYNAMIC \
         -DHAVE_STRING_H -DHAVE_SNPRINTF -DHAVE_STDARG_H -DHAVE_VPRINTF \
         -I${HOME}/gawk-4.1.0 -D${ARCH}

LDFLAGS = -shared

.PHONY : test all clean

test: cawk.so test.so
	${HOME}/gawk-4.1.0/gawk -f test.awk

all: cawk.so

cawk.so: cawk.c makefile
	gcc cawk.c ${CFLAGS} -o cawk.o
	gcc cawk.o ${LDFLAGS} -lffi -o cawk.so

CFLAGS2 = -Wall -fPIC -shared -g -c -O2 

test.so: test.c makefile
	gcc test.c ${CFLAGS2} -o test.o
	gcc test.o ${LDFLAGS} -o test.so

clean:
	rm *.o *.so
