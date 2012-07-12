#include "protocol/firefly_protocol_private.h"

#include <labcomm.h>

#include <firefly_errors.h>


void labcomm_error_to_ff_error(enum labcomm_error error_id, size_t nbr_va_args, ...)
{
	const char *err_msg = labcomm_error_get_str(error_id); // The final string to print.
	if (err_msg == NULL) {
		err_msg = "Error with an unknown error ID occured.";
	}
	firefly_error(FIREFLY_ERROR_LABCOMM, 1, err_msg);
}
