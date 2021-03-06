#define _POSIX_C_SOURCE (200112L)
#include <pthread.h>
#include "test/test_resend_posix.h"

#include <stdlib.h>
#include <string.h>
#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include <time.h>

#include "utils/firefly_resend_posix.h"

#define DATA_SIZE (5)

static unsigned char data[DATA_SIZE] = {1,2,3,4,5};

unsigned char *data_test_new()
{
	unsigned char *d = malloc(DATA_SIZE);
	memcpy(d, data, DATA_SIZE);
	return d;
}

int setup_resend_posix()
{
	return 0;
}

int teardown_resend_posix()
{
	return 0;
}

static inline long timespec_diff_ms(struct timespec *f, struct timespec *t)
{
	return (t->tv_sec - f->tv_sec)*1000 + (t->tv_nsec - f->tv_nsec)/1000000;
}

void test_add_simple()
{
	struct timespec at;
	struct resend_queue *rq = firefly_resend_queue_new();
	CU_ASSERT_PTR_NULL(rq->first);
	CU_ASSERT_PTR_NULL(rq->last);

	unsigned char id = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			1500, 1, NULL);
	clock_gettime(CLOCK_REALTIME, &at);
	CU_ASSERT_TRUE(id != 0);
	CU_ASSERT_PTR_NOT_NULL(rq->first);
	CU_ASSERT_PTR_NOT_NULL(rq->last);
	CU_ASSERT_PTR_EQUAL(rq->first, rq->last);
	CU_ASSERT_NOT_EQUAL(id, rq->next_id);
	struct resend_elem *re = rq->first;
	CU_ASSERT_EQUAL(re->id, id);
	CU_ASSERT_EQUAL(re->size, DATA_SIZE);
	CU_ASSERT_EQUAL(memcmp(data, re->data, DATA_SIZE), 0);
	long diff = timespec_diff_ms(&at, &re->resend_at);
	CU_ASSERT_TRUE(diff <= 1500);
	CU_ASSERT_TRUE(diff > 1470);
	CU_ASSERT_PTR_NULL(re->prev);
	
	firefly_resend_queue_free(rq);
}

void test_remove_empty()
{
	struct resend_queue *rq = firefly_resend_queue_new();

	CU_ASSERT_PTR_NULL(rq->first);
	CU_ASSERT_PTR_NULL(rq->last);
	unsigned char id = 1;
	firefly_resend_remove(rq, id);
	CU_ASSERT_PTR_NULL(rq->first);
	CU_ASSERT_PTR_NULL(rq->last);

	firefly_resend_queue_free(rq);
}

/* Depends on test_add_simple
 */
void test_remove_simple()
{
	struct resend_queue *rq = firefly_resend_queue_new();

	unsigned char id = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			1500, 1, NULL);
	firefly_resend_remove(rq, id);
	CU_ASSERT_PTR_NULL(rq->first);
	CU_ASSERT_PTR_NULL(rq->last);

	firefly_resend_queue_free(rq);
}

/* Depends on test_add_simple
 */
void test_add_many()
{
	struct resend_queue *rq = firefly_resend_queue_new();

	unsigned char id_1 = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 1, NULL);
	CU_ASSERT_TRUE(id_1 != 0);
	CU_ASSERT_PTR_EQUAL(rq->first, rq->last);
	CU_ASSERT_NOT_EQUAL(id_1, rq->next_id);
	struct resend_elem *re = rq->first;
	CU_ASSERT_EQUAL(re->id, id_1);
	CU_ASSERT_PTR_NULL(re->prev);

	unsigned char id_2 = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 1, NULL);
	CU_ASSERT_TRUE(id_2 != 0);
	CU_ASSERT_NOT_EQUAL(id_1, id_2);
	CU_ASSERT_NOT_EQUAL(id_2, rq->next_id);
	CU_ASSERT_PTR_NOT_EQUAL(rq->first, rq->last);
	CU_ASSERT_PTR_EQUAL(re->prev, rq->last);
	re = rq->last;
	CU_ASSERT_PTR_NULL(re->prev);

	unsigned char id_3 = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 1, NULL);
	CU_ASSERT_TRUE(id_3 != 0);
	CU_ASSERT_NOT_EQUAL(id_1, id_3);
	CU_ASSERT_NOT_EQUAL(id_2, id_3);
	CU_ASSERT_NOT_EQUAL(id_3, rq->next_id);
	CU_ASSERT_PTR_EQUAL(re->prev, rq->last);
	re = rq->last;
	CU_ASSERT_PTR_NULL(re->prev);

	firefly_resend_queue_free(rq);
}

/* Depends on test_add_simple
 */
void test_remove_many()
{
	struct resend_queue *rq = firefly_resend_queue_new();

	unsigned char id_1 = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 1, NULL);
	unsigned char id_2 = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 1, NULL);
	unsigned char id_3 = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 1, NULL);
	firefly_resend_remove(rq, id_1);
	CU_ASSERT_PTR_NOT_NULL(rq->first);
	CU_ASSERT_PTR_NOT_NULL(rq->last);
	firefly_resend_remove(rq, id_2);
	CU_ASSERT_PTR_NOT_NULL(rq->first);
	CU_ASSERT_PTR_NOT_NULL(rq->last);
	CU_ASSERT_PTR_EQUAL(rq->first, rq->last);
	firefly_resend_remove(rq, id_3);
	CU_ASSERT_PTR_NULL(rq->first);
	CU_ASSERT_PTR_NULL(rq->last);

	firefly_resend_queue_free(rq);
}

/* Depends on test_add_simple
 */
void test_remove_not_ordered()
{
	struct resend_queue *rq = firefly_resend_queue_new();

	unsigned char id_1 = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 1, NULL);
	unsigned char id_2 = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 1, NULL);
	unsigned char id_3 = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 1, NULL);
	firefly_resend_remove(rq, id_2);
	CU_ASSERT_PTR_NOT_NULL(rq->first);
	CU_ASSERT_PTR_NOT_NULL(rq->last);
	firefly_resend_remove(rq, id_1);
	CU_ASSERT_PTR_NOT_NULL(rq->first);
	CU_ASSERT_PTR_NOT_NULL(rq->last);
	CU_ASSERT_PTR_EQUAL(rq->first, rq->last);
	firefly_resend_remove(rq, id_3);
	CU_ASSERT_PTR_NULL(rq->first);
	CU_ASSERT_PTR_NULL(rq->last);

	firefly_resend_queue_free(rq);
}

void test_remove_non_existing_elem()
{
	struct resend_queue *rq = firefly_resend_queue_new();

	unsigned char id = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 1, NULL);

	firefly_resend_remove(rq, id + 1);
	CU_ASSERT_PTR_NOT_NULL(rq->first);
	CU_ASSERT_PTR_NOT_NULL(rq->last);
	firefly_resend_remove(rq, id);
	CU_ASSERT_PTR_NULL(rq->first);
	CU_ASSERT_PTR_NULL(rq->last);

	firefly_resend_queue_free(rq);
}

void test_top_simple()
{
	struct resend_queue *rq = firefly_resend_queue_new();

	unsigned char id = firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 1, NULL);

	struct resend_elem *elem = firefly_resend_top(rq);
	CU_ASSERT_PTR_NOT_NULL(rq->first);
	CU_ASSERT_PTR_NOT_NULL(rq->last);
	CU_ASSERT_EQUAL(elem->id, id);

	firefly_resend_remove(rq, id);
	CU_ASSERT_PTR_NULL(rq->first);
	CU_ASSERT_PTR_NULL(rq->last);

	firefly_resend_queue_free(rq);
}

void test_readd_simple()
{
	struct resend_queue *rq = firefly_resend_queue_new();

	firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 2, NULL);
	struct resend_elem *re = rq->first;
	re->resend_at.tv_sec = 1;
	re->resend_at.tv_nsec = 2;
	re->timeout = 500;
	firefly_resend_readd(rq, re->id);

	CU_ASSERT_EQUAL(re->num_retries, 1);
	CU_ASSERT_EQUAL(re->resend_at.tv_sec, 1);
	CU_ASSERT_EQUAL(re->resend_at.tv_nsec, 500000002);
	CU_ASSERT_EQUAL(rq->first, re);
	CU_ASSERT_EQUAL(rq->first, rq->last);


	firefly_resend_queue_free(rq);
}

void test_readd_complex()
{
	struct resend_queue *rq = firefly_resend_queue_new();

	firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 2, NULL);
	firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 1, NULL);
	firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 2, NULL);
	struct resend_elem *re = rq->first;
	re->resend_at.tv_sec = 1;
	re->resend_at.tv_nsec = 600000002;
	re->timeout = 2500;
	firefly_resend_readd(rq, re->id);

	CU_ASSERT_EQUAL(re->num_retries, 1);
	CU_ASSERT_EQUAL(re->resend_at.tv_sec, 4);
	CU_ASSERT_EQUAL(re->resend_at.tv_nsec, 100000002);
	CU_ASSERT_NOT_EQUAL(rq->first, re);
	CU_ASSERT_EQUAL(rq->last, re);

	firefly_resend_queue_free(rq);
}

void test_readd_removed()
{
	struct resend_queue *rq = firefly_resend_queue_new();

	firefly_resend_add(rq, data_test_new(), DATA_SIZE,
			0, 2, NULL);
	struct resend_elem *re = rq->first;
	struct timespec t = re->resend_at;
	re->timeout = 500;
	firefly_resend_readd(rq, 5);

	CU_ASSERT_EQUAL(re->num_retries, 2);
	CU_ASSERT_EQUAL(re->resend_at.tv_sec, t.tv_sec);
	CU_ASSERT_EQUAL(re->resend_at.tv_nsec, t.tv_nsec);
	CU_ASSERT_EQUAL(rq->first, re);
	CU_ASSERT_EQUAL(rq->first, rq->last);

	firefly_resend_queue_free(rq);
}

int main()
{
	CU_pSuite resend_posix = NULL;

	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	resend_posix = CU_add_suite("resend_posix", setup_resend_posix, teardown_resend_posix);
	if (resend_posix == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	if (
		(CU_add_test(resend_posix, "test_add_simple",
				test_add_simple) == NULL)
			   ||
		(CU_add_test(resend_posix, "test_remove_simple",
				test_remove_simple) == NULL)
			   ||
		(CU_add_test(resend_posix, "test_add_many",
				test_add_many) == NULL)
			   ||
		(CU_add_test(resend_posix, "test_remove_many",
				test_remove_many) == NULL)
			   ||
		(CU_add_test(resend_posix, "test_remove_empty",
				test_remove_empty) == NULL)
			   ||
		(CU_add_test(resend_posix, "test_remove_not_ordered",
				test_remove_not_ordered) == NULL)
			   ||
		(CU_add_test(resend_posix, "test_remove_non_existing_elem",
				test_remove_non_existing_elem) == NULL)
			   ||
		(CU_add_test(resend_posix, "test_top_simple",
				test_top_simple) == NULL)
			   ||
		(CU_add_test(resend_posix, "test_readd_simple",
				test_readd_simple) == NULL)
			   ||
		(CU_add_test(resend_posix, "test_readd_complex",
				test_readd_complex) == NULL)
			   ||
		(CU_add_test(resend_posix, "test_readd_removed",
				test_readd_removed) == NULL)
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
}
