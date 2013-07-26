/**
 * @file
 * @brief Test the event queue functionallity.
 */

#include "CUnit/Basic.h"
#include "CUnit/Console.h"

#include <stdio.h>

#include <utils/firefly_event_queue.h>

#include "utils/firefly_event_queue_private.h"

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
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, 1, NULL);

	int st = q->offer_event_cb(q, 1, NULL, NULL, 0, NULL);
	CU_ASSERT_EQUAL(st, 1);
	CU_ASSERT_PTR_NOT_NULL_FATAL(q->head);
	CU_ASSERT_PTR_NOT_NULL_FATAL(q->head->depends);
	struct firefly_event *test = firefly_event_pop(q);

	CU_ASSERT_EQUAL(1, test->prio);

	CU_ASSERT_PTR_NULL(q->head);

	// Clean up
	firefly_event_return(q, &test);
	firefly_event_queue_free(&q);
	CU_ASSERT_PTR_NULL(q);
}

void test_queue_events_in_order()
{
	int st;
	struct firefly_event *ev;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, 3, NULL);

	st = q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_EQUAL(st, 1);
	CU_ASSERT_EQUAL(q->head->id, 1);
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_MEDIUM, NULL, NULL, 0, NULL);
	CU_ASSERT_EQUAL(st, 2);
	CU_ASSERT_EQUAL(q->head->id, 2);
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_HIGH, NULL, NULL, 0, NULL);
	CU_ASSERT_EQUAL(st, 3);
	CU_ASSERT_EQUAL(q->head->id, 3);

	ev = firefly_event_pop(q);
	CU_ASSERT_EQUAL(FIREFLY_PRIORITY_HIGH, ev->prio);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_EQUAL(FIREFLY_PRIORITY_MEDIUM, ev->prio);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_EQUAL(FIREFLY_PRIORITY_LOW, ev->prio);
	firefly_event_return(q, &ev);

	CU_ASSERT_PTR_NULL(q->head);

	firefly_event_queue_free(&q);
}

void test_queue_event_same_prio()
{

	int st;
	struct firefly_event *ev;
	int id_1, id_2, id_3;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, 3, NULL);

	st = q->offer_event_cb(q, FIREFLY_PRIORITY_MEDIUM, NULL, &id_1, 0, NULL);
	CU_ASSERT_EQUAL(st, 1);
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_MEDIUM, NULL, &id_2, 0, NULL);
	CU_ASSERT_EQUAL(st, 2);
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_MEDIUM, NULL, &id_3, 0, NULL);
	CU_ASSERT_EQUAL(st, 3);

	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_1, ev->context);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_2, ev->context);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_3, ev->context);
	firefly_event_return(q, &ev);

	CU_ASSERT_PTR_NULL(q->head);

	firefly_event_queue_free(&q);
}

void test_queue_event_same_and_diff_prio()
{

	int st;
	struct firefly_event *ev;
	int id_l1, id_l2, id_m1, id_m2, id_h1, id_h2;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, 6, NULL);

	st = q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, &id_l1, 0, NULL);
	CU_ASSERT_EQUAL(st, 1);
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, &id_l2, 0, NULL);
	CU_ASSERT_EQUAL(st, 2);
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_MEDIUM, NULL, &id_m1, 0, NULL);
	CU_ASSERT_EQUAL(st, 3);
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_MEDIUM, NULL, &id_m2, 0, NULL);
	CU_ASSERT_EQUAL(st, 4);
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_HIGH, NULL, &id_h1, 0, NULL);
	CU_ASSERT_EQUAL(st, 5);
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_HIGH, NULL, &id_h2, 0, NULL);
	CU_ASSERT_EQUAL(st, 6);

	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_h1, ev->context);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_h2, ev->context);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_m1, ev->context);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_m2, ev->context);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_l1, ev->context);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_l2, ev->context);
	firefly_event_return(q, &ev);

	CU_ASSERT_PTR_NULL(q->head);

	firefly_event_queue_free(&q);
}

void test_pop_empty()
{
	struct firefly_event *ev;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, 3, NULL);

	CU_ASSERT_PTR_NULL(firefly_event_pop(q));

	int st = q->offer_event_cb(q, 1, NULL, NULL, 0, NULL);
	CU_ASSERT_EQUAL(st, 1);
	st = q->offer_event_cb(q, 1, NULL, NULL, 0, NULL);
	CU_ASSERT_EQUAL(st, 2);
	st = q->offer_event_cb(q, 1, NULL, NULL, 0, NULL);
	CU_ASSERT_EQUAL(st, 3);

	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);

	CU_ASSERT_PTR_NULL(firefly_event_pop(q));

	firefly_event_queue_free(&q);
}

void test_length()
{
	const size_t k_len = 256;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, k_len, NULL);

	for (size_t i = 0; i < k_len; i++) {
		q->offer_event_cb(q, 1, NULL, NULL, 0, NULL);
	}

	CU_ASSERT_EQUAL(firefly_event_queue_length(q), k_len);

	firefly_event_queue_free(&q);
}

static int complex_event(void *event_arg)
{
	static int last_id = 0;
	int id = *((int*) event_arg);
	int expected_id = 0;
	switch (last_id) {
		case 0:
			expected_id = 1;
			break;
		case 1:
			expected_id = 2;
			break;
		case 2:
			expected_id = 4;
			break;
		case 3:
			expected_id = 7;
			break;
		case 4:
			expected_id = 5;
			break;
		case 5:
			expected_id = 6;
			break;
		case 6:
			expected_id = 3;
			break;
		case 7:
			expected_id = 0;
			break;
	}
	CU_ASSERT_EQUAL(expected_id, id);
	if (expected_id != id) {
		printf("exp: %d, id: %d\n", expected_id, id);
	}
	last_id = id;
	return 0;
}

void test_complex_priorities()
{
	struct firefly_event *ev;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, 7, NULL);

	int id_1 = 1;
	int id_2 = 2;
	int id_3 = 3;
	int id_4 = 4;
	int id_5 = 5;
	int id_6 = 6;
	int id_7 = 7;

	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, complex_event, &id_1, 0, NULL);
	ev = firefly_event_pop(q);
	firefly_event_execute(ev);
	firefly_event_return(q, &ev);
	q->offer_event_cb(q, FIREFLY_PRIORITY_MEDIUM, complex_event, &id_2, 0, NULL);
	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, complex_event, &id_3, 0, NULL);
	ev = firefly_event_pop(q);
	firefly_event_execute(ev);
	firefly_event_return(q, &ev);
	q->offer_event_cb(q, FIREFLY_PRIORITY_HIGH, complex_event, &id_4, 0, NULL);
	q->offer_event_cb(q, FIREFLY_PRIORITY_HIGH, complex_event, &id_5, 0, NULL);
	q->offer_event_cb(q, FIREFLY_PRIORITY_MEDIUM, complex_event, &id_6, 0, NULL);
	ev = firefly_event_pop(q);
	firefly_event_execute(ev);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	firefly_event_execute(ev);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	firefly_event_execute(ev);
	firefly_event_return(q, &ev);
	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, complex_event, &id_7, 0, NULL);
	ev = firefly_event_pop(q);
	firefly_event_execute(ev);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	firefly_event_execute(ev);
	firefly_event_return(q, &ev);

	firefly_event_queue_free(&q);
}

void test_event_pool_simple()
{
	struct firefly_event *ev;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, 1, NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(q->event_pool);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[0]);

	// Test take and return an event
	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_PTR_NULL(q->event_pool[0]);
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[0]);

	// Same test again to test reusability of event
	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_PTR_NULL(q->event_pool[0]);
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[0]);

	firefly_event_queue_free(&q);
}

void test_event_pool_three()
{
	struct firefly_event *ev;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, 3, NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(q->event_pool);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[0]);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[1]);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[2]);

	// Test take three events
	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_PTR_NULL(q->event_pool[0]);
	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_PTR_NULL(q->event_pool[1]);
	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_PTR_NULL(q->event_pool[2]);

	// Test return one event and take it again
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[2]);
	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_PTR_NULL(q->event_pool[2]);

	// Test return two events and take them again
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[2]);
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[1]);
	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_PTR_NULL(q->event_pool[1]);
	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_PTR_NULL(q->event_pool[2]);

	// Test return all events
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[2]);
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[1]);
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[0]);

	firefly_event_queue_free(&q);
}

void test_event_pool_resize()
{
	struct firefly_event *ev;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, 1, NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(q->event_pool);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[0]);

	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_PTR_NULL(q->event_pool[0]);

	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_EQUAL_FATAL(2, q->event_pool_size);
	CU_ASSERT_PTR_NULL(q->event_pool[1]);

	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_EQUAL_FATAL(4, q->event_pool_size);
	CU_ASSERT_PTR_NULL(q->event_pool[2]);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[3]);

	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_EQUAL(4, q->event_pool_size);
	CU_ASSERT_PTR_NULL(q->event_pool[3]);

	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_EQUAL_FATAL(8, q->event_pool_size);
	CU_ASSERT_PTR_NULL(q->event_pool[4]);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[5]);

	// test return the events again
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[4]);
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[3]);
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[2]);
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[1]);
	ev = firefly_event_pop(q);
	firefly_event_return(q, &ev);
	CU_ASSERT_PTR_NOT_NULL(q->event_pool[0]);

	// test add one to make sure they were returned ok
	q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, NULL, 0, NULL);
	CU_ASSERT_PTR_NULL(q->event_pool[0]);

	// Cleanup
	firefly_event_queue_free(&q);
}

void test_event_dependencies()
{
	int st;
	int64_t id;
	struct firefly_event *ev;
	int id_1 = 1;
	int id_2 = 2;
	int id_3 = 3;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, 3, NULL);

	st = q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, &id_1, 0, NULL);
	CU_ASSERT_EQUAL(st, 1);
	CU_ASSERT_EQUAL(q->head->id, 1);
	id = st;
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_MEDIUM, NULL, &id_2, 1, &id);
	CU_ASSERT_EQUAL(st, 2);
	CU_ASSERT_EQUAL(q->head->id, 2);
	id = st;
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_HIGH, NULL, &id_3, 1, &id);
	CU_ASSERT_EQUAL(st, 3);
	CU_ASSERT_EQUAL(q->head->id, 3);

	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_1, ev->context);
	CU_ASSERT_EQUAL(FIREFLY_PRIORITY_LOW, ev->prio);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_2, ev->context);
	CU_ASSERT_EQUAL(FIREFLY_PRIORITY_MEDIUM, ev->prio);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_3, ev->context);
	CU_ASSERT_EQUAL(FIREFLY_PRIORITY_HIGH, ev->prio);
	firefly_event_return(q, &ev);

	CU_ASSERT_PTR_NULL(q->head);

	firefly_event_queue_free(&q);
}

void test_event_dependencies_done()
{
	int st;
	int64_t id;
	struct firefly_event *ev;
	int id_1 = 1;
	int id_2 = 2;
	int id_3 = 3;
	int64_t dep_1[] = {4};
	int64_t dep_2[] = {id_1, 5};
	int64_t dep_3[] = {id_2, 6};
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, 3, NULL);

	st = q->offer_event_cb(q, FIREFLY_PRIORITY_LOW, NULL, &id_1, 1, dep_1);
	CU_ASSERT_EQUAL(st, 1);
	CU_ASSERT_EQUAL(q->head->id, 1);
	id = st;
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_MEDIUM, NULL, &id_2, 2, dep_2);
	CU_ASSERT_EQUAL(st, 2);
	CU_ASSERT_EQUAL(q->head->id, 2);
	id = st;
	st = q->offer_event_cb(q, FIREFLY_PRIORITY_HIGH, NULL, &id_3, 2, dep_3);
	CU_ASSERT_EQUAL(st, 3);
	CU_ASSERT_EQUAL(q->head->id, 3);

	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_1, ev->context);
	CU_ASSERT_EQUAL(FIREFLY_PRIORITY_LOW, ev->prio);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_2, ev->context);
	CU_ASSERT_EQUAL(FIREFLY_PRIORITY_MEDIUM, ev->prio);
	firefly_event_return(q, &ev);
	ev = firefly_event_pop(q);
	CU_ASSERT_PTR_EQUAL(&id_3, ev->context);
	CU_ASSERT_EQUAL(FIREFLY_PRIORITY_HIGH, ev->prio);
	firefly_event_return(q, &ev);

	CU_ASSERT_PTR_NULL(q->head);

	firefly_event_queue_free(&q);
}

// TODO test errors when using event pool
int main()
{
	CU_pSuite event_suite = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	event_suite = CU_add_suite("event_posix", init_suite_event,
			clean_suite_event);
	if (event_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Transport UDP POSIX tests.
	if (
		(CU_add_test(event_suite, "test_add_pop_event_simple",
				test_add_pop_event_simple) == NULL)
		||
		(CU_add_test(event_suite, "test_queue_events_in_order",
				test_queue_events_in_order) == NULL)
		||
		(CU_add_test(event_suite, "test_queue_event_same_prio",
					 test_queue_event_same_prio) == NULL)
		||
		(CU_add_test(event_suite, "test_queue_event_same_and_diff_prio",
					 test_queue_event_same_prio) == NULL)
		||
		(CU_add_test(event_suite, "test_pop_empty",
				test_pop_empty) == NULL)
		||
		(CU_add_test(event_suite, "test_length",
					 test_length) == NULL)
		||
		(CU_add_test(event_suite, "test_complex_priorities",
					 test_complex_priorities) == NULL)
		||
		(CU_add_test(event_suite, "test_event_pool_simple",
					 test_event_pool_simple) == NULL)
		||
		(CU_add_test(event_suite, "test_event_pool_three",
					 test_event_pool_three) == NULL)
		||
		(CU_add_test(event_suite, "test_event_pool_resize",
					 test_event_pool_resize) == NULL)
		||
		(CU_add_test(event_suite, "test_event_dependencies",
					 test_event_dependencies) == NULL)
		||
		(CU_add_test(event_suite, "test_event_dependencies_done",
					 test_event_dependencies_done) == NULL)
	   ) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Set verbosity.
	CU_basic_set_mode(CU_BRM_VERBOSE);
	/*CU_console_run_tests();*/

	// Run all test suites.
	CU_basic_run_tests();
	int res = CU_get_number_of_tests_failed();
	// Clean up.
	CU_cleanup_registry();

	if (res != 0) {
		return 1;
	}
	return CU_get_error();
}
