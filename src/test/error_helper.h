#ifndef ERROR_HELPER_H
#define ERROR_HELPER_H

#include <stdbool.h>

#include <utils/firefly_errors.h>

extern bool was_in_error;
extern enum firefly_error expected_error;

// Override.
void firefly_error(enum firefly_error error_id, size_t nbr_va_args, ...);
#endif
