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
typedef bool (*application_on_conn_recv_cb)(struct connection *conn);

/**
 * @brief A data structure representing a linked list of connections.
 */
struct llp_connection_list_node {
	struct llp_connection_list_node *next;
	struct connection *conn;
};

/**
 * @brief A general data structure representing a link layer port on the
 * transport layer.
 */
struct transport_llp {
	application_on_conn_recv_cb on_conn_recv; /**< The callback to be called
							when a new connection is
							detected. */
	struct llp_connection_list_node *conn_list; /**< A linked list of
							connections. */
	void *llp_platspec; /**< Platform, and transport method, specific data.
				*/
};	

#endif
