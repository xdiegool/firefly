#ifndef PONG_PTCP_H
#define PONG_PTCP_H

#include <pthread.h>
#include <stdbool.h>

struct thread_arg {
	pthread_mutex_t m;
	pthread_cond_t t;
	bool started;
};

/**
 * @brief Start the pong test.
 */
void *pong_main_thread(void *arg);

#endif
