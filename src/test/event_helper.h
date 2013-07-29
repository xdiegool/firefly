#ifndef TEST_EVENT_HELPER_H
#define TEST_EVENT_HELPER_H

#include <utils/firefly_event_queue.h>

#define TEST_MAX_NBR_EVENTS (50)

void event_execute_test(struct firefly_event_queue *eq, size_t num);

void event_execute_all_test(struct firefly_event_queue *eq);

int64_t mock_test_event_add(struct firefly_event_queue *eq, unsigned char prio,
		firefly_event_execute_f execute, void *context,
		unsigned int nbr_depends, const int64_t *depends);

void mock_test_event_queue_reset(struct firefly_event_queue *eq);

#endif
