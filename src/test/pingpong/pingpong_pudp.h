
#ifndef PINGPONG_PUDP_H
#define PINGPONG_PUDP_H

#include <eventqueue/event_queue.h>
#include <transport/firefly_transport_udp_posix.h>

#define PING_PORT (55555)
#define PING_ADDR ("127.0.0.1")
#define PONG_PORT (55556)
#define PONG_ADDR ("127.0.0.1")

struct event_queue_signals {
	pthread_mutex_t eq_lock;
	pthread_cond_t eq_cond;
};

void reader_thread_main(struct firefly_transport_llp *llp);

int event_add_mutex(struct firefly_event_queue *eq, struct firefly_event *ev);

void *event_thread_main(void *args);
#endif
