LDLIBS=-lpthread -lrt
CC=clang
CFLAGS= -g

.PHONY: all

all: posix_test xeno_test

posix_test.o: test.c
	$(CC) -c $(CFLAGS) -o $@ $<

posix_test: posix_test.o

xeno_test.o: test.c
	$(CC) -c $(CFLAGS) \
		-I/usr/include/xenomai -I/usr/include/xenomai/posix \
		-D_GNU_SOURCE -D_REENTRANT -D__XENO__ \
		-o $@ $<

xeno_test: xeno_test.o
	$(CC) $^ \
		-Wl,@/usr/lib/posix.wrappers \
		-L/usr/lib \
		-lpthread_rt -lxenomai -lpthread -lrt \
		-o $@

clean:
	$(RM) posix_test.o posix_test xeno_test.o xeno_test
