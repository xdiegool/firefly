/**
 * @file
 * @brief Private transport types and functions that are common to all
 * transport implementations.
 */

#ifndef FIREFLY_TRANSPORT_PRIVATE_H
#define FIREFLY_TRANSPORT_PRIVATE_H

#include <stdbool.h>

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

/**
 * @brief Compare a connection to some value. This funktion is used by
 * find_connection to find a connection.
 *
 * @param conn The connection to compare.
 * @param context The data to compare the connection to.
 * @retval true if the context is considered identifying the connection.
 * @retval false otherwise.
 */
typedef bool (*conn_eq_f)(struct firefly_connection *conn, void *context);

/**
 * @brief Finds the first \c struct #firefly_connection for which the equals
 * funktion returns true.
 *
 * Find the first connection for which the equals funktion returns true
 * associated with the supplied \a llp. The connection which is found is
 * returned, if none is found \c NULL is returned.
 *
 * @param llp The link layer port to search for the #firefly_connection in.
 * @param context The context passed to the equals funktion.
 * @param conn_eq The equals funktion used to compare connections.
 * @return The #firefly_connection which is found.
 * @retval NULL is returned if no connection was found.
 * @retval struct #firefly_connection* is if it was found.
 */
struct firefly_connection *find_connection(struct firefly_transport_llp *llp,
		void *context, conn_eq_f conn_eq);

#endif
