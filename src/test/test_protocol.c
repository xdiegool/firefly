/**
 * @file
 * @brief Test the functionality of the protocol layer.
 */
#include <stdio.h>

#include "CUnit/Basic.h"
#include "CUnit/Console.h"

int init_suit()
{
	return 0; // Success.
}

int clean_suit()
{
	return 0; // Success.
}

void test_encode_protocol()
{
	CU_ASSERT(1 == 1);
}

int main()
{
	CU_pSuite pSuite = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	// Add our test suites.
	pSuite = CU_add_suite("basic", init_suit, clean_suit);
	if (pSuite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	
	if (
		(CU_add_test(pSuite, "test_encode_protocol", test_encode_protocol) == NULL)
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
