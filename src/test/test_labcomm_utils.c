#include "test/test_labcomm_utils.h"

#include <unistd.h>
#include <fcntl.h>

#include <CUnit/Basic.h>

#include "utils/firefly_errors.h"
#include "utils/cppmacros.h"

void handle_labcomm_error(enum labcomm_error error_id, size_t nbr_va_args, ...)
{
	UNUSED_VAR(nbr_va_args);
	const char *err_msg = labcomm_error_get_str(error_id);
	if (err_msg == NULL) {
		err_msg = "Error with an unknown error ID occured.";
	}
	firefly_error(FIREFLY_ERROR_LABCOMM, 1, err_msg);

	CU_FAIL_FATAL("Labcomm error occured!\n");
}
