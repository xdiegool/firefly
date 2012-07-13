/**
 * @file
 * @brief Private transport types and functions that are common to all
 * transport implementations.
 */

#ifndef FIREFLY_TRANSPORT_PRIVATE_H
#define FIREFLY_TRANSPORT_PRIVATE_H

/**
 * @struct transport_llp
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

/**
 * @struct llp_connection_list_node
 * @brief A data structure representing a linked list of connections.
 */
struct llp_connection_list_node {
	struct llp_connection_list_node *next;
	struct firefly_connection *conn;
};

#endif
