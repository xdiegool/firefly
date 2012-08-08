/**
 * @file
 * @brief General transport related data structures and functions.
 */

#ifndef FIREFLY_TRANSPORT_H
#define FIREFLY_TRANSPORT_H

#include <protocol/firefly_protocol.h>
#include <stdbool.h>


/**
 * @brief This callback will be called when a new connection is received.
 *
 * This function is implemented by the application layer. It will be called
 * when the transport layer receives a new connection. The return value of this
 * function specifies to the underlying layers whether to accept the connection
 * or refuse it, true will accept it and false will refuse it.
 *
 * @param conn The received connection.
 * @return Return true to accept the connection and false to refuse it.
 */
typedef bool (*firefly_on_conn_recv)(struct firefly_connection *conn);

/**
 * @brief A opaque general data structure representing a \c link \c layer \c port on the
 * transport layer.
 */
struct firefly_transport_llp;

#endif
