#include <pthread.h>
#include "test/pingpong/pingpong_pudp.h"

#include <utils/firefly_event_queue.h>
#include <transport/firefly_transport_udp_posix.h>

#include "utils/cppmacros.h"


void pingpong_test_init(struct pingpong_test *test, char *name)
{
	test->pass = false;
	test->name = name;
}

void pingpong_test_pass(struct pingpong_test *test)
{
	test->pass = true;
}

void pingpong_test_print_results(struct pingpong_test *tests, size_t nbr_tests)
{
	printf("======TEST RESULTS=======\n");
	for (size_t i = 0; i < nbr_tests; i++) {
		printf("phase %d: %s...%s\n", i, tests[i].name,
				tests[i].pass ? "passed" : "failed");
	}
}

void *reader_thread_main(void *args)
{
	struct firefly_transport_llp *llp = (struct firefly_transport_llp *) args;
	// TODO add something here to make it stop or something
	while (1) {
		firefly_transport_udp_posix_read(llp);
	}

}

int event_add_mutex(struct firefly_event_queue *eq, struct firefly_event *ev)
{
	int res = 0;
	struct event_queue_signals *eq_s =
		(struct event_queue_signals *)firefly_event_queue_get_context(eq);

	res = pthread_mutex_lock(&eq_s->eq_lock);
	if (res) {
		return res;
	}
	res = firefly_event_add(eq, ev);
	if (!res) {
		pthread_cond_signal(&eq_s->eq_cond);
	}
	pthread_mutex_unlock(&eq_s->eq_lock);
	return res;
}

void *event_thread_main(void *args)
{
	struct firefly_event_queue *eq = (struct firefly_event_queue *) args;
	// TODO eq
	struct event_queue_signals *eq_s =
		(struct event_queue_signals *)
		firefly_event_queue_get_context(eq);
	struct firefly_event *ev = NULL;

	// TODO consider another expression besides '1'
	while (1) {
		pthread_mutex_lock(&eq_s->eq_lock);
		while (firefly_event_queue_length(eq) < 1) {
			pthread_cond_wait(&eq_s->eq_cond, &eq_s->eq_lock);
		}
		ev = firefly_event_pop(eq);
		pthread_mutex_unlock(&eq_s->eq_lock);
		firefly_event_execute(ev);
	}
	return NULL;
}
