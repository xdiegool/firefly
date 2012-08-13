/**
 * @file
 * @brief Private transport types and functions that are common to all
 * transport implementations.
 */

#ifndef FIREFLY_TRANSPORT_PRIVATE_H
#define FIREFLY_TRANSPORT_PRIVATE_H

/**
 * @brief A general data structure representing a link layer port on the
 * transport layer.
 */
struct firefly_transport_llp {
	struct llp_connection_list_node *conn_list; /**< A linked list of
							connections. */
	void *llp_platspec; /**< Platform, and transport method, specific data.
				*/
};

/**
 * @brief A data structure representing a linked list of connections.
 */
struct llp_connection_list_node {
	struct llp_connection_list_node *next;
	struct firefly_connection *conn;
};

int firefly_transport_connection_udp_posix_free_event(void *event_arg);

#endif
