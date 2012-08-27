#include <utils/firefly_errors.h>

#include <stdarg.h>
#include <stdio.h>

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
