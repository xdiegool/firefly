#ifndef FIREFLY_EVENT_QUEUE_POSIX_H
#define FIREFLY_EVENT_QUEUE_POSIX_H

#include <pthread.h>

#include <utils/firefly_event_queue.h>

struct firefly_event_queue_posix_context {
	pthread_mutex_t lock;
	pthread_cond_t signal;
	pthread_t event_loop;
	bool event_loop_stop;
};

struct firefly_event_queue *firefly_event_queue_posix_new();
void firefly_event_queue_posix_free(struct firefly_event_queue **eq);

int firefly_event_queue_posix_add(struct firefly_event_queue *eq,
		unsigned char prio, firefly_event_execute_f execute, void *context);

int firefly_event_queue_posix_run(struct firefly_event_queue *eq,
		pthread_attr_t *attr);
int firefly_event_queue_posix_stop(struct firefly_event_queue *eq);

#endif
