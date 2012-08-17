
#ifndef PINGPONG_PUDP_H
#define PINGPONG_PUDP_H

#include <utils/firefly_event_queue.h>
#include <transport/firefly_transport_udp_posix.h>

#define PING_PORT (55555)
#define PING_ADDR ("127.0.0.1")
#define PONG_PORT (55556)
#define PONG_ADDR ("127.0.0.1")

#define PONG_DATA (2222)
#define PING_DATA (1111)

struct event_queue_signals {
	pthread_mutex_t eq_lock;
	pthread_cond_t eq_cond;
};

struct pingpong_test {
	char *name;
	bool pass;
};

void pingpong_test_init(struct pingpong_test *test, char *name);

void pingpong_test_pass(struct pingpong_test *test);

void pingpong_test_print_results(struct pingpong_test *tests, size_t nbr_tests);

void *reader_thread_main(void *args);

int event_add_mutex(struct firefly_event_queue *eq, struct firefly_event *ev);

void *event_thread_main(void *args);
#endif
