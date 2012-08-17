/**
 * @file
 * @brief Test the event queue functionallity.
 */

#include "CUnit/Basic.h"
#include "CUnit/Console.h"

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
		firefly_event_queue_new(firefly_event_add, NULL);

	struct firefly_event *ev = firefly_event_new(1, NULL, NULL);
	int st = q->offer_event_cb(q, ev);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *test = firefly_event_pop(q);

	CU_ASSERT_PTR_EQUAL(ev, test);
	CU_ASSERT_EQUAL(ev->prio, test->prio);

	CU_ASSERT_PTR_NULL(q->head);

	// Clean up
	firefly_event_free(&ev);
	firefly_event_queue_free(&q);
	CU_ASSERT_PTR_NULL(q);
}

void test_queue_events_in_order()
{
	int st;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, NULL);

	struct firefly_event *ev_low = firefly_event_new(
			FIREFLY_PRIORITY_LOW, NULL, NULL);
	st = q->offer_event_cb(q, ev_low);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_med = firefly_event_new(
			FIREFLY_PRIORITY_MEDIUM, NULL, NULL);
	st = q->offer_event_cb(q, ev_med);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_high = firefly_event_new(
			FIREFLY_PRIORITY_HIGH, NULL, NULL);
	st = q->offer_event_cb(q, ev_high);
	CU_ASSERT_EQUAL(st, 0);

	CU_ASSERT_PTR_EQUAL(ev_high, firefly_event_pop(q));
	CU_ASSERT_PTR_EQUAL(ev_med, firefly_event_pop(q));
	CU_ASSERT_PTR_EQUAL(ev_low, firefly_event_pop(q));

	CU_ASSERT_PTR_NULL(q->head);

	firefly_event_free(&ev_low);
	firefly_event_free(&ev_med);
	firefly_event_free(&ev_high);
	firefly_event_queue_free(&q);
}

void test_queue_event_same_prio()
{

	int st;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, NULL);

	struct firefly_event *ev_1 = firefly_event_new(
			FIREFLY_PRIORITY_MEDIUM, NULL, NULL);
	st = q->offer_event_cb(q, ev_1);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_2 = firefly_event_new(
			FIREFLY_PRIORITY_MEDIUM, NULL, NULL);
	st = q->offer_event_cb(q, ev_2);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_3 = firefly_event_new(
			FIREFLY_PRIORITY_MEDIUM, NULL, NULL);
	st = q->offer_event_cb(q, ev_3);
	CU_ASSERT_EQUAL(st, 0);

	CU_ASSERT_PTR_EQUAL(ev_1, firefly_event_pop(q));
	CU_ASSERT_PTR_EQUAL(ev_2, firefly_event_pop(q));
	CU_ASSERT_PTR_EQUAL(ev_3, firefly_event_pop(q));

	CU_ASSERT_PTR_NULL(q->head);

	firefly_event_free(&ev_1);
	firefly_event_free(&ev_2);
	firefly_event_free(&ev_3);
	firefly_event_queue_free(&q);
}

void test_queue_event_same_and_diff_prio()
{

	int st;
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, NULL);

	struct firefly_event *ev_low1 = firefly_event_new(
			FIREFLY_PRIORITY_LOW, NULL, NULL);
	st = q->offer_event_cb(q, ev_low1);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_low2 = firefly_event_new(
			FIREFLY_PRIORITY_LOW, NULL, NULL);
	st = q->offer_event_cb(q, ev_low2);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_med1 = firefly_event_new(
			FIREFLY_PRIORITY_MEDIUM, NULL, NULL);
	st = q->offer_event_cb(q, ev_med1);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_med2 = firefly_event_new(
			FIREFLY_PRIORITY_MEDIUM, NULL, NULL);
	st = q->offer_event_cb(q, ev_med2);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_high1 = firefly_event_new(
			FIREFLY_PRIORITY_HIGH, NULL, NULL);
	st = q->offer_event_cb(q, ev_high1);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_high2 = firefly_event_new(
			FIREFLY_PRIORITY_HIGH, NULL, NULL);
	st = q->offer_event_cb(q, ev_high2);
	CU_ASSERT_EQUAL(st, 0);

	CU_ASSERT_PTR_EQUAL(ev_high1, firefly_event_pop(q));
	CU_ASSERT_PTR_EQUAL(ev_high2, firefly_event_pop(q));
	CU_ASSERT_PTR_EQUAL(ev_med1, firefly_event_pop(q));
	CU_ASSERT_PTR_EQUAL(ev_med2, firefly_event_pop(q));
	CU_ASSERT_PTR_EQUAL(ev_low1, firefly_event_pop(q));
	CU_ASSERT_PTR_EQUAL(ev_low2, firefly_event_pop(q));

	CU_ASSERT_PTR_NULL(q->head);

	firefly_event_free(&ev_low1);
	firefly_event_free(&ev_low2);
	firefly_event_free(&ev_med1);
	firefly_event_free(&ev_med2);
	firefly_event_free(&ev_high1);
	firefly_event_free(&ev_high2);
	firefly_event_queue_free(&q);
}

void test_pop_empty()
{
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, NULL);

	CU_ASSERT_PTR_NULL(firefly_event_pop(q));

	struct firefly_event *ev_1 = firefly_event_new(1, NULL, NULL);
	int st = q->offer_event_cb(q, ev_1);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_2 = firefly_event_new(2, NULL, NULL);
	st = q->offer_event_cb(q, ev_2);
	CU_ASSERT_EQUAL(st, 0);
	struct firefly_event *ev_3 = firefly_event_new(3, NULL, NULL);
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
	struct firefly_event_queue *q =
		firefly_event_queue_new(firefly_event_add, NULL);

	for (int i = 0; i < k_len; i++) {
		struct firefly_event *ev = firefly_event_new(1, NULL, NULL);
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
