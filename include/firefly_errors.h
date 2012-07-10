#ifndef FIREFLY_ERRORS_H
#define FIREFLY_ERRORS_H

#include <stdlib.h>

typedef void (*error_handler)(char *err_msg);

/* Error IDs */
enum firefly_error {
	FIREFLY_ERROR_FIRST, 		// _must_ be the first enum element. firefly_error_get_str() depends on this.
	FIREFLY_ERROR_ALLOC, 
	FIREFLY_ERROR_SOCKET, 
	FIREFLY_ERROR_USER_DEF,
	FIREFLY_ERROR_LAST		// _must_ be the last enum element. firefly_error_get_str() depends on this.
};

/* Error strings. _must_ be the same order as in enum firefly_error */
extern const char *firefly_error_strings[];

/* Get a string describing the supplied standrad labcomm error. */
const char *firefly_error_get_str(enum firefly_error error_id);

/* Link with another implementation of this if you don't like fprintfs.
   Default error handler, prints message to stderr. 
   Extra info about the error can be supplied as char* as VA-args. Especially user defined errors should supply a describing string. if nbr_va_args > 1 the first variable argument must be a printf format string and the possibly following arguments are passed as va_args to vprintf. 
   */
void firefly_error(enum firefly_error error_id, size_t nbr_va_args, ...);

#endif
