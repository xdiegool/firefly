#include <stdio.h>

#include "CUnit/Basic.h"
#include "CUnit/Console.h"

int init_suit()
{
	printf("Initializing suit.\n");
	return 0; // Success.
}

int clean_suit()
{
	printf("Cleaning suit.\n");
	return 0; // Success.
}

void test_1()
{
	CU_ASSERT(1 == 1);
}

void test_2()
{
	CU_ASSERT(2 == 2);
}

void test_str()
{
	CU_ASSERT_STRING_EQUAL("str", "str");
	int err_code = CU_get_error();
	printf("errcode=%d\n", err_code);
}

void test_str_fail()
{
	/*CU_ASSERT_STRING_EQUAL("str", "rts");*/
	/*// ... failed*/
	/*int err_code = CU_get_error();*/
	/*printf("errcode=%d\n", err_code);*/

	/*CU_FAIL("failstr");*/

}

int main()
{
	CU_pSuite pSuite = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	// Add our test suites.
	pSuite = CU_add_suite("Suite1", init_suit, clean_suit);
	if (pSuite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	
	if (
		(CU_add_test(pSuite, "test_1", test_1) == NULL)
	       	||
		(CU_add_test(pSuite, "test_2", test_2) == NULL)
	       	||
		(CU_add_test(pSuite, "test_str", test_str) == NULL)
	       	||
		(CU_add_test(pSuite, "test_str_fail", test_str_fail) == NULL)
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
