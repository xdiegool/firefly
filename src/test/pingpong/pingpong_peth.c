#include <pthread.h>
#include "test/pingpong/pingpong_peth.h"

#include <transport/firefly_transport_eth_posix.h>

#include <stdbool.h>
#include <sys/time.h>

void *reader_thread_main(void *args)
{
#ifdef __XENO__
	int err = pthread_set_mode_np(0, PTHREAD_WARNSW | PTHREAD_PRIMARY);
	if (err != 0) {
		printf("Error set mode %d\n", err);
		return NULL;
	}
#endif
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
