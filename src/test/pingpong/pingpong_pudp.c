#include <pthread.h>
#include "test/pingpong/pingpong_pudp.h"

#include <eventqueue/event_queue.h>
#include <transport/firefly_transport_udp_posix.h>


void reader_thread_main(struct firefly_transport_llp *llp)
{
	// TODO add something here to make it stop or something
	while (1) {
		printf("Reading next packet.\n");
		firefly_transport_udp_posix_read(llp);
	}

}

int event_add_mutex(struct firefly_event_queue *eq, struct firefly_event *ev)
{
	int res = 0;
	struct event_queue_signals *eq_s =
		(struct event_queue_signals *) eq->context;

	printf("EVENT: Adding event.\n");
	res = pthread_mutex_lock(&eq_s->eq_lock);
	if (res) {
		return res;
	}
	res = firefly_event_add(eq, ev);
	if (!res) {
		printf("EVENT: Signaling.\n");
		pthread_cond_signal(&eq_s->eq_cond);
	}
	pthread_mutex_unlock(&eq_s->eq_lock);
	return res;
}

void *event_thread_main(void *args)
{
	struct firefly_event_queue *eq = (struct firefly_event_queue *) args;
	struct event_queue_signals *eq_s =
		(struct event_queue_signals *) eq->context;
	struct firefly_event *ev = NULL;
	printf("EVENT: Executing events...\n");

	// TODO consider another expression besides '1'
	while (1) {
		pthread_mutex_lock(&eq_s->eq_lock);
		while (firefly_event_queue_length(eq) < 1) {
			printf("EVENT: Empty queue, waiting.\n");
			pthread_cond_wait(&eq_s->eq_cond, &eq_s->eq_lock);
			printf("EVENT: Got signal.\n");
		}
		ev = firefly_event_pop(eq);
		pthread_mutex_unlock(&eq_s->eq_lock);
		printf("EVENT: Executing event.\n");
		firefly_event_execute(ev);
	}
	return NULL;
}
