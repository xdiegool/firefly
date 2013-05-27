#include "CUnit/Basic.h"

#include <utils/firefly_event_queue.h>

void event_execute_test(struct firefly_event_queue *eq, size_t num)
{
	struct firefly_event *ev = NULL;
	while (num > 0) {
		ev = firefly_event_pop(eq);
		CU_ASSERT_PTR_NOT_NULL_FATAL(ev);
		firefly_event_execute(ev);
		--num;
	}
}

void event_execute_all_test(struct firefly_event_queue *eq)
{
	struct firefly_event *ev = firefly_event_pop(eq);
	while (ev != NULL) {
		CU_ASSERT_PTR_NOT_NULL_FATAL(ev);
		firefly_event_execute(ev);
		ev = firefly_event_pop(eq);
	}
}
