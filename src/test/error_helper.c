#include <stdbool.h>
#include <CUnit/Basic.h>

#include <utils/firefly_errors.h>
#include "utils/cppmacros.h"


bool was_in_error = false;
enum firefly_error expected_error;

void firefly_error(enum firefly_error error_id, size_t nbr_va_args, ...)
{
	UNUSED_VAR(nbr_va_args);
	was_in_error = true;
	/*printf("Error: %d\n", error_id);*/
	/*if (nbr_va_args >= 1) {*/
		/*va_list arg_pointer;*/
		/*va_start(arg_pointer, nbr_va_args);*/
		/*char *print_format = va_arg(arg_pointer, char *);*/
		/*vfprintf(stderr, print_format, arg_pointer);*/
	/*}*/
	CU_ASSERT_EQUAL(expected_error, error_id);
}
