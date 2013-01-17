#ifndef PONG_PMULTI_H
#define PONG_PMULTI_H

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
