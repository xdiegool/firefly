/**
 * @file
 * @brief Error constants and handler functions.
 */

#ifndef FIREFLY_ERRORS_H
#define FIREFLY_ERRORS_H

#include <stdlib.h>

typedef void (*error_handler)(char *err_msg);

/**
 * @enum
 * @breif  Error IDs
 */
enum firefly_error {
	FIREFLY_ERROR_FIRST, 		/**< \b Must be the first enum element. firefly_error_get_str() depends on this.*/
	FIREFLY_ERROR_ALLOC,
	FIREFLY_ERROR_SOCKET,
	FIREFLY_ERROR_USER_DEF,
	FIREFLY_ERROR_LAST		/**< \b Must be the last enum element. firefly_error_get_str() depends on this.*/
};

/**
 * @var Error strings. _must_ be the same order as in enum firefly_error
 */
extern const char *firefly_error_strings[];

/**
 * @brief Get a string describing the specified firefly \a error_id.
 * @param error_id The error id that a string is requested for.
 * @return A text string describing the error.
 * @retval NULL Is returned if the \a error_id is not recognized.
 */
const char *firefly_error_get_str(enum firefly_error error_id);

/**
 * @brief The error handler taking some action on the specified \a error_id.
 * The default implementation of this function will print an error message to
 * \e stderr for the specified \a error_id. Optionally any number of \e va_args can be
 * passed. The default implementation expects the first to be a \e pritnf format
 * string and any parameters following that to be parameters to that format
 * string. When the user defined \a error_id is used the va_args can have another
 * meaning.
 * This function will no do anything if \e FIREFLY_NO_STDIO is defined.
 *
 * @param error_id The ID of the error to handle.
 * @param nbr_va_args The number of \e va_args used. Provide 0 if none are
 * supplied.
 * @param ... Variable number of arguments.
 */
void firefly_error(enum firefly_error error_id, size_t nbr_va_args, ...);

#endif
