#include <utils/firefly_errors.h>

#include <stdarg.h>
#ifdef ARM_CORTEXM3_CODESOURCERY
	#include <ustdlib.h> // Needed for usnprintf and uvsprintf that replaces
			     // the regular print functions.
#else
	#include <stdio.h>
#endif

#define MAX_ERR_LEN (1024)	/* Maximum length of error strings. */

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

// TODO use this for errors.
void labcomm_error_to_ff_error(enum labcomm_error error_id, size_t nbr_va_args,
									...)
{
	char *err_msg = malloc(MAX_ERR_LEN);
	if (err_msg == NULL) {
		err_msg = "Failed to allocate memory for error message.";
		firefly_error(FIREFLY_ERROR_ALLOC, 1, err_msg);
		return;
	}

	const char *lc_err_msg = labcomm_error_get_str(error_id);
	if (lc_err_msg == NULL) {
		free(err_msg);
		err_msg = "Error with an unknown error ID occured.";
		firefly_error(FIREFLY_ERROR_LABCOMM, 1, err_msg);
		return;
	}

	size_t chars_left = MAX_ERR_LEN;
	int chars_written;
	chars_written = snprintf(err_msg, chars_left, "%s\n", lc_err_msg);
	if (chars_written == -1) {
		exit(EXIT_FAILURE); // Error in error function. We're screwed.
	} 
	chars_left -= chars_written;

	if (nbr_va_args > 0) {
		va_list arg_pointer;
		va_start(arg_pointer, nbr_va_args);

		chars_written = snprintf(err_msg, chars_left, "%s\n",
					"Extra info {");
		if (chars_written == -1) {
			// Error in error function. We're screwed.
			exit(EXIT_FAILURE); 
		} 
		chars_left -= chars_written;

		char *print_format = va_arg(arg_pointer, char *);
		chars_written = vsnprintf(err_msg, chars_left, print_format,
				arg_pointer);
		if (chars_written < 0) {
			// Error in error function. We're screwed.
			exit(EXIT_FAILURE); 
		}
		chars_left -= chars_written;

		fprintf(stderr, "}\n");
		chars_written = snprintf(err_msg, chars_left, "}\n");
		if (chars_written == -1) {
			// Error in error function. We're screwed.
			exit(EXIT_FAILURE); 
		} 
		chars_left -= chars_written;

		va_end(arg_pointer);
	}

	firefly_error(FIREFLY_ERROR_LABCOMM, 1, err_msg);
	free(err_msg);
}
