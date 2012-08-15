#include <firefly_errors.h>

// Some projects can not use stdio.h.
#include <stdio.h>
#include <stdarg.h>

/**
 * @var firefly_error_strings
 * @brief Error strings.
 *
 * \b _Must_ be the same order as in enum firefly_error.
 */
const char *firefly_error_strings[] = {
	"First guard. Don't use this \"error\".",
	"Memory allocation failed.",
	"Socket operation failed.",
	"Binding LLP failed.",
	"Failed to prase IP address.",
	"Error writing data to transport medium.",
	"LabComm error has occured.",
	"Invalid protocol state occured.",
	"User defined error.",
	"User has not set callback.",
	"Event type does not exist.",
	"End guard. Don't use this \"error\".",
};

const char *firefly_error_get_str(enum firefly_error error_id)
{
	const char *error_str = NULL;
	// Check if this is a known error ID.
	if (error_id > FIREFLY_ERROR_FIRST && error_id < FIREFLY_ERROR_LAST) {
		error_str = firefly_error_strings[error_id];
	}
	return error_str;
}

void firefly_error(enum firefly_error error_id, size_t nbr_va_args, ...)
{
	/* The final string to print. */
	const char *err_msg = firefly_error_get_str(error_id);

	if (err_msg == NULL) {
		err_msg = "Error with an unknown error ID occurred.";
	}
	fprintf(stderr, "%s\n", err_msg);

	if (nbr_va_args > 0) {
		va_list arg_pointer;
		va_start(arg_pointer, nbr_va_args);

		fprintf(stderr, "%s\n", "Extra info {");
		char *print_format = va_arg(arg_pointer, char *);
		vfprintf(stderr, print_format, arg_pointer);
		fprintf(stderr, "}\n");

		va_end(arg_pointer);
	}
}
