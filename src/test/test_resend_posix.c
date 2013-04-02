#include <pthread.h>
#include "test/test_resend_posix.h"

#include <stdlib.h>
#include <string.h>
#include "CUnit/Basic.h"
#include "CUnit/Console.h"

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

void test_add_simple()
{
	struct timespec at;
	at.tv_sec = 1;
	at.tv_nsec = 2;
	struct resend_queue *rq = firefly_resend_queue_new();
	CU_ASSERT_PTR_NULL(rq->first);
	CU_ASSERT_PTR_NULL(rq->last);

	unsigned char id = firefly_resend_add(rq, data_test_new(), DATA_SIZE, at, NULL);
	CU_ASSERT_TRUE(id != 0);
	CU_ASSERT_PTR_NOT_NULL(rq->first);
	CU_ASSERT_PTR_NOT_NULL(rq->last);
	CU_ASSERT_PTR_EQUAL(rq->first, rq->last);
	CU_ASSERT_NOT_EQUAL(id, rq->next_id);
	struct resend_elem *re = rq->first;
	CU_ASSERT_EQUAL(re->id, id);
	CU_ASSERT_EQUAL(re->size, DATA_SIZE);
	CU_ASSERT_EQUAL(memcmp(data, re->data, DATA_SIZE), 0);
	CU_ASSERT_EQUAL(at.tv_sec, re->resend_at.tv_sec);
	CU_ASSERT_EQUAL(at.tv_nsec, re->resend_at.tv_nsec);
	CU_ASSERT_PTR_NULL(re->prev);
	
	firefly_resend_queue_free(rq);
}

void test_remove_empty()
{
	struct timespec at;
	at.tv_sec = 1;
	at.tv_nsec = 2;
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
	struct timespec at;
	at.tv_sec = 1;
	at.tv_nsec = 2;
	struct resend_queue *rq = firefly_resend_queue_new();

	unsigned char id = firefly_resend_add(rq, data_test_new(), DATA_SIZE, at, NULL);
	firefly_resend_remove(rq, id);
	CU_ASSERT_PTR_NULL(rq->first);
	CU_ASSERT_PTR_NULL(rq->last);

	firefly_resend_queue_free(rq);
}

/* Depends on test_add_simple
 */
void test_add_many()
{
	struct timespec at_1;
	at_1.tv_sec = 1;
	at_1.tv_nsec = 2;
	struct timespec at_2;
	at_2.tv_sec = 2;
	at_2.tv_nsec = 2;
	struct timespec at_3;
	at_3.tv_sec = 3;
	at_3.tv_nsec = 2;
	struct resend_queue *rq = firefly_resend_queue_new();

	unsigned char id_1 = firefly_resend_add(rq, data_test_new(), DATA_SIZE, at_1, NULL);
	CU_ASSERT_TRUE(id_1 != 0);
	CU_ASSERT_PTR_EQUAL(rq->first, rq->last);
	CU_ASSERT_NOT_EQUAL(id_1, rq->next_id);
	struct resend_elem *re = rq->first;
	CU_ASSERT_EQUAL(re->id, id_1);
	CU_ASSERT_PTR_NULL(re->prev);

	unsigned char id_2 = firefly_resend_add(rq, data_test_new(), DATA_SIZE, at_2, NULL);
	CU_ASSERT_TRUE(id_2 != 0);
	CU_ASSERT_NOT_EQUAL(id_1, id_2);
	CU_ASSERT_NOT_EQUAL(id_2, rq->next_id);
	CU_ASSERT_PTR_NOT_EQUAL(rq->first, rq->last);
	CU_ASSERT_PTR_EQUAL(re->prev, rq->first);
	re = rq->first;
	CU_ASSERT_PTR_NULL(re->prev);

	unsigned char id_3 = firefly_resend_add(rq, data_test_new(), DATA_SIZE, at_3, NULL);
	CU_ASSERT_TRUE(id_3 != 0);
	CU_ASSERT_NOT_EQUAL(id_1, id_3);
	CU_ASSERT_NOT_EQUAL(id_2, id_3);
	CU_ASSERT_NOT_EQUAL(id_3, rq->next_id);
	CU_ASSERT_PTR_EQUAL(re->prev, rq->first);
	re = rq->first;
	CU_ASSERT_PTR_NULL(re->prev);

	firefly_resend_queue_free(rq);
}

/* Depends on test_add_simple
 */
void test_remove_many()
{
	struct timespec at;
	at.tv_sec = 1;
	at.tv_nsec = 2;
	struct resend_queue *rq = firefly_resend_queue_new();

	unsigned char id_1 = firefly_resend_add(rq, data_test_new(), DATA_SIZE, at, NULL);
	unsigned char id_2 = firefly_resend_add(rq, data_test_new(), DATA_SIZE, at, NULL);
	unsigned char id_3 = firefly_resend_add(rq, data_test_new(), DATA_SIZE, at, NULL);
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
	struct timespec at;
	at.tv_sec = 1;
	at.tv_nsec = 2;
	struct resend_queue *rq = firefly_resend_queue_new();

	unsigned char id_1 = firefly_resend_add(rq, data_test_new(), DATA_SIZE, at, NULL);
	unsigned char id_2 = firefly_resend_add(rq, data_test_new(), DATA_SIZE, at, NULL);
	unsigned char id_3 = firefly_resend_add(rq, data_test_new(), DATA_SIZE, at, NULL);
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
