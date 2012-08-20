/**
 * @file
 * @brief Common C pre processor macros.
 */

#ifndef CPP_MACROS_H
#define CPP_MACROS_H

#define X_EMPTY(mac) var ## 1
#define EMPTY(mac) X_EMPTY(mac)	/* Returns 1 if macro mac is empty. */

#define UNUSED_VAR(x)	((void)(x))	/* Supress unsued variable compiler
					   warings. */

#endif
