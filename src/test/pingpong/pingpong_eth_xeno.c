#include "test/pingpong/pingpong_eth_xeno.h"

#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <native/mutex.h>
#include <native/task.h>

#include <transport/firefly_transport_eth_xeno.h>

void reader_thread_main(void *args)
{
	int err = rt_task_set_mode(0, T_WARNSW, NULL);
	if (err != 0) {
		printf("Error set mode %d\n", err);
		return;
	}
	struct reader_thread_args *rtarg = (struct reader_thread_args *) args;
	struct firefly_transport_llp *llp = rtarg->llp;
	bool finish = false;
	int64_t timeout;
	timeout = 1000000000;
	while (!finish) {
		firefly_transport_eth_xeno_read(llp, &timeout);
		rt_mutex_acquire(&rtarg->lock, timeout);
		finish = rtarg->finish;
		rt_mutex_release(&rtarg->lock);
	}
}
