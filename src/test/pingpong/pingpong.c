#include <pthread.h>
#include "test/pingpong/pingpong.h"

#include <stdio.h>

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
