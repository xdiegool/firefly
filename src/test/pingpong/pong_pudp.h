#ifndef PONG_PUDP_H
#define PONG_PUDP_H

#include <pthread.h>

#define PONG_NBR_TESTS (7)

struct thread_arg {
	pthread_mutex_t m;
	pthread_cond_t t;
};

/**
 * @brief Start the pong test.
 */
void *pong_main_thread(void *arg);

#endif
