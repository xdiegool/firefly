#include <pthread.h>
#include "test/pingpong/pingpong_peth.h"

#include <transport/firefly_transport_eth_posix.h>

#include <stdbool.h>
#include <sys/time.h>

void *reader_thread_main(void *args)
{
	struct reader_thread_args *rtarg = (struct reader_thread_args *) args;
	struct firefly_transport_llp *llp = rtarg->llp;
	bool finish = false;
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	// TODO add something here to make it stop or something
	while (!finish) {
		firefly_transport_eth_posix_read(llp, &tv);
		pthread_mutex_lock(&rtarg->lock);
		finish = rtarg->finish;
		pthread_mutex_unlock(&rtarg->lock);
	}

	pthread_exit(NULL);
}
