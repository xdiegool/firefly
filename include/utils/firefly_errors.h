/**
 * @file
 * @brief Error constants and handler functions.
 */

#ifndef FIREFLY_ERRORS_H
#define FIREFLY_ERRORS_H

#include <stdlib.h>
#include <labcomm.h>

		/* firefly_error(FIREFLY_ERROR_SOCKET, 3, */
		/* 		"Failed in %s() on line %d.\n", __FUNCTION__, */
		/* 		__LINE__); */

#define FFL(fe)							\
	do { firefly_error(fe, 3, ":%s() failed at %s:%04d\n",	\
			   __func__, __FILE__, __LINE__);	\
	} while (0)

#define FFLIF(cond, fe)						\
	do { if (cond) FFL(fe); } while (0);


/**
 * @brief Error IDs
 */
enum firefly_error {
	FIREFLY_ERROR_FIRST,
	/**< \b Must be the first enum element. firefly_error_get_str() depends on this.*/
	FIREFLY_ERROR_ALLOC,
	/**< Represents an error while allocating memory. */
	FIREFLY_ERROR_SOCKET,
	/**< Something went wrong with a socker operation. */
	FIREFLY_ERROR_LLP_BIND,
	/**< Failed to parse an IP address. */
	FIREFLY_ERROR_IP_PARSE,
	/**< Failed to write data at transport medium.. */
	FIREFLY_ERROR_TRANS_WRITE,
	/**< Represents an error while opening, binding or performing similar operations on a socket. */
	FIREFLY_ERROR_LABCOMM,
	/**< An LabComm error has occured. */
	FIREFLY_ERROR_PROTO_STATE,
	/**< An LabComm error has occured. */
	FIREFLY_ERROR_USER_DEF,
	/**< Represents a user defined error. */
	FIREFLY_ERROR_MISSING_CALLBACK,
	/**< User has not set callback */
	FIREFLY_ERROR_EVENT,
	/**< Error relating to events */
	FIREFLY_ERROR_CONN_STATE,
	/**< Error due to bad state of a connection. */
	FIREFLY_ERROR_CHAN_REFUSED,
	/**< Error remote end refused channel request. */
	FIREFLY_ERROR_LAST
	/**< \b Must be the last enum element. firefly_error_get_str() depends on this.*/
};

/**
 * @brief Get a string describing the specified firefly \a error_id.
 * @param error_id The error id that a string is requested for.
 * @return A text string describing the error.
 * @retval NULL Is returned if the \a error_id is not recognized.
 */
const char *firefly_error_get_str(enum firefly_error error_id);

/**
 * @brief The error handler taking some action on the specified \a error_id.
 *
 * The default implementation of this function will print an error message to
 * \e stderr for the specified \a error_id. Optionally any number of \e va_args
 * can be passed. The default implementation expects the first to be a \e
 * pritnf format string and any parameters following that to be parameters to
 * that format string. When the user defined \a error_id is used the va_args
 * can have another meaning.
 *
 * @param error_id The ID of the error to handle.
 * @param nbr_va_args The number of \e va_args used. Provide 0 if none are
 * supplied.
 * @param ... Variable number of arguments.
 */
void firefly_error(enum firefly_error error_id, size_t nbr_va_args, ...);

/**
 * @brief Error callback used in LabComm that forwards the error to the \e
 * firefly_error handler that is in use.
 * @param error_id The LabComm error that occured.
 * @param nbr_va_args The number of va_args that are passed.
 * @param ... Some va_args.
 */
void labcomm_error_to_ff_error(enum labcomm_error error_id, size_t nbr_va_args,
									...);

#endif
