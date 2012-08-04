
#include "CUnit/Basic.h"
#include "CUnit/Console.h"

#include "eventqueue/event_queue.h"

int init_suite_event()
{
	return 0;
}

int clean_suite_event()
{
	return 0;
}

void test_add_pop_event_simple()
{
	struct firefly_event_queue *q = firefly_event_queue_new(firefly_event_add);

	struct firefly_event *ev = firefly_event_new(3, 1);
	int st = q->offer_event_cb(q, ev);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *test = firefly_event_pop(q);

	CU_ASSERT_PTR_EQUAL(ev, test);
	CU_ASSERT_EQUAL(ev->base.type, test->base.type);
	CU_ASSERT_EQUAL(ev->base.prio, test->base.prio);

	CU_ASSERT_PTR_NULL(q->head);
	CU_ASSERT_PTR_NULL(q->tail);

	// Clean up
	firefly_event_free(&ev);
	firefly_event_queue_free(&q);
	CU_ASSERT_PTR_NULL(q);
}

void test_queue_events_in_order()
{
	struct firefly_event_queue *q = firefly_event_queue_new(firefly_event_add);

	struct firefly_event *ev_1 = firefly_event_new(1, 1);
	int st = q->offer_event_cb(q, ev_1);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_2 = firefly_event_new(2, 2);
	st = q->offer_event_cb(q, ev_2);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_3 = firefly_event_new(3, 3);
	st = q->offer_event_cb(q, ev_3);
	CU_ASSERT_EQUAL(st, 0);

	CU_ASSERT_PTR_EQUAL(ev_1, firefly_event_pop(q));
	CU_ASSERT_PTR_EQUAL(ev_2, firefly_event_pop(q));
	CU_ASSERT_PTR_EQUAL(ev_3, firefly_event_pop(q));

	CU_ASSERT_PTR_NULL(q->head);
	CU_ASSERT_PTR_NULL(q->tail);

	firefly_event_free(&ev_1);
	firefly_event_free(&ev_2);
	firefly_event_free(&ev_3);
	firefly_event_queue_free(&q);
}

void test_pop_empty()
{
	struct firefly_event_queue *q = firefly_event_queue_new(firefly_event_add);

	CU_ASSERT_PTR_NULL(firefly_event_pop(q));

	struct firefly_event *ev_1 = firefly_event_new(1, 1);
	int st = q->offer_event_cb(q, ev_1);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_2 = firefly_event_new(2, 2);
	st = q->offer_event_cb(q, ev_2);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_3 = firefly_event_new(3, 3);
	st = q->offer_event_cb(q, ev_3);
	CU_ASSERT_EQUAL(st, 0);

	firefly_event_pop(q);
	firefly_event_pop(q);
	firefly_event_pop(q);

	CU_ASSERT_PTR_NULL(firefly_event_pop(q));

	firefly_event_free(&ev_1);
	firefly_event_free(&ev_2);
	firefly_event_free(&ev_3);
	firefly_event_queue_free(&q);
}

void test_length()
{
	const int k_len = 256;
	struct firefly_event_queue *q = firefly_event_queue_new(firefly_event_add);

	for (int i = 0; i < k_len; i++) {
		struct firefly_event *ev = firefly_event_new(1, 1);
		q->offer_event_cb(q, ev);
	}

	CU_ASSERT_EQUAL(firefly_event_queue_length(q), k_len);

	firefly_event_queue_free(&q);
}

int main()
{
	CU_pSuite event_suite = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	event_suite = CU_add_suite("event_posix", init_suite_event, clean_suite_event);
	if (event_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Transport UDP Posix tests.
	if (
		(CU_add_test(event_suite, "test_add_pop_event_simple",
				test_add_pop_event_simple) == NULL)
			   ||
		(CU_add_test(event_suite, "test_queue_events_in_order",
				test_queue_events_in_order) == NULL)
			   ||
		(CU_add_test(event_suite, "test_pop_empty",
				test_pop_empty) == NULL)
		||
		(CU_add_test(event_suite, "test_length",
					 test_length) == NULL)
	   ) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Set verbosity.
	CU_basic_set_mode(CU_BRM_VERBOSE);
	/*CU_console_run_tests();*/

	// Run all test suites.
	CU_basic_run_tests();


	// Clean up.
	CU_cleanup_registry();

	return CU_get_error();
}
