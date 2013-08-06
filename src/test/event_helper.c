#include "test/event_helper.h"

#include "CUnit/Basic.h"

#include <utils/firefly_event_queue.h>
#include <utils/cppmacros.h>

unsigned int nbr_added_events = 0;
int64_t test_event_ids[50];
int64_t test_event_deps[50][FIREFLY_EVENT_QUEUE_MAX_DEPENDS];

void event_execute_test(struct firefly_event_queue *eq, size_t num)
{
	struct firefly_event *ev = NULL;
	while (num > 0) {
		ev = firefly_event_pop(eq);
		CU_ASSERT_PTR_NOT_NULL_FATAL(ev);
		firefly_event_execute(ev);
		firefly_event_return(eq, &ev);
		--num;
	}
}

void event_execute_all_test(struct firefly_event_queue *eq)
{
	struct firefly_event *ev = firefly_event_pop(eq);
	while (ev != NULL) {
		CU_ASSERT_PTR_NOT_NULL_FATAL(ev);
		firefly_event_execute(ev);
		firefly_event_return(eq, &ev);
		ev = firefly_event_pop(eq);
	}
}

int64_t mock_test_event_add(struct firefly_event_queue *eq, unsigned char prio,
		firefly_event_execute_f execute, void *context,
		unsigned int nbr_depends, const int64_t *depends)
{
	int64_t ret;
	ret = firefly_event_add(eq, prio, execute, context, nbr_depends, depends);
	test_event_ids[nbr_added_events] = ret;
	for (unsigned int i = 0; i < nbr_depends; i++) {
		test_event_deps[nbr_added_events][i] = depends[i];
	}
	++nbr_added_events;
	if (nbr_added_events > TEST_MAX_NBR_EVENTS)
		mock_test_event_queue_reset(eq);
	return ret;
}

void mock_test_event_queue_reset(struct firefly_event_queue *eq)
{
	UNUSED_VAR(eq);
	nbr_added_events = 0;
	memset(test_event_ids, 0, sizeof(test_event_ids));
	memset(test_event_deps, 0, sizeof(test_event_deps));
}
