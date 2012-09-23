/**
 * @file
 * @brief Private transport types and functions that are common to all
 * transport implementations.
 */

#ifndef FIREFLY_TRANSPORT_PRIVATE_H
#define FIREFLY_TRANSPORT_PRIVATE_H

#define FIREFLY_CONNECTION_OPEN (1)
#define FIREFLY_CONNECTION_CLOSED (0)


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

/**
 * @brief The event free'ing a firefly_connection and all it's channels.
 *
 * @param event_arg The firefly_connection to free.
 */
int firefly_transport_connection_udp_posix_free_event(void *event_arg);

/**
 * @brief Adds a connection to the connection list in \a llp.
 *
 * @param conn The connection to add.
 * @param llp The link layer port structure to add the connection to.
 */
void add_connection_to_llp(struct firefly_connection *conn,
		struct firefly_transport_llp *llp);

#endif
