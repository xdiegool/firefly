
#ifndef PINGPONG_PETH_H
#define PINGPONG_PETH_H

#include <pthread.h>
#include <stdbool.h>

struct reader_thread_args {
	pthread_mutex_t lock;
	bool finish;
	struct firefly_transport_llp *llp;
};

void *reader_thread_main(void *args);
#endif
