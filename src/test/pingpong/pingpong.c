#include <pthread.h>
#include "test/pingpong/pingpong.h"

#include <utils/firefly_event_queue.h>

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

void pingpong_test_print_results(struct pingpong_test *tests, size_t nbr_tests,
		char *test_suite_name)
{
	printf("======%s test results=======\n", test_suite_name);
	size_t nbr_success = 0;
	for (size_t i = 0; i < nbr_tests; i++) {
		printf("phase %zu: %s...%s\n", i, tests[i].name,
				tests[i].pass ? "passed" : "failed");
		if (tests[i].pass) {
			++nbr_success;
		}
	}
	printf("Summary: %zu/%zu succeeded. %zu failures.\n", nbr_success, nbr_tests,
		(nbr_tests - nbr_success));
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
	int event_left = 0;
	bool finish = eq_s->event_exec_finish;

	// TODO consider another expression besides '1'
	while (!finish || event_left > 0) {
		pthread_mutex_lock(&eq_s->eq_lock);
		event_left = firefly_event_queue_length(eq);
		while (event_left < 1 && !finish) {
			pthread_cond_wait(&eq_s->eq_cond, &eq_s->eq_lock);
			finish = eq_s->event_exec_finish;
			event_left = firefly_event_queue_length(eq);
		}
		ev = firefly_event_pop(eq);
		pthread_mutex_unlock(&eq_s->eq_lock);
		if (ev != NULL) {
			firefly_event_execute(ev);
		}
	}
	return NULL;
}