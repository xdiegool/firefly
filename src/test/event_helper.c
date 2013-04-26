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
