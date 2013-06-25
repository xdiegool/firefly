
#ifndef PINGPONG_PETH_H
#define PINGPONG_PETH_H

#include <native/mutex.h>
#include <stdbool.h>

struct reader_thread_args {
	RT_MUTEX lock;
	bool finish;
	struct firefly_transport_llp *llp;
};

void reader_thread_main(void *args);
#endif
