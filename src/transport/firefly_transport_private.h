/**
 * @file
 * @brief Private transport types and functions that are common to all
 * transport implementations.
 */

#ifndef FIREFLY_TRANSPORT_PRIVATE_H
#define FIREFLY_TRANSPORT_PRIVATE_H

#include <stdbool.h>
#include <protocol/firefly_protocol_private.h>

/**
 * @brief The different states a llp may have. Used to enable multiple
 * asynchronous actions which depends on each other.
 */
enum firefly_llp_state {
	FIREFLY_LLP_OPEN, /**< Defines the open state of a \a llp. */
	FIREFLY_LLP_CLOSING /**< Defines the closed state of a \a llp. */
};

/**
 * @brief A general data structure representing a link layer port on the
 * transport layer.
 */
struct firefly_transport_llp {
	enum firefly_llp_state state; /**< The state of the llp. Subject to
									interpretation of each specific transport
									layer. */
	struct llp_connection_list_node *conn_list; /**< A linked list of all
												  connections. */
	void *llp_platspec; /**< Platform, and transport method, specific data. */
	protocol_data_received_f protocol_data_received_cb; /** The function which
														  passes data received
														  by the transport
														  layer. Replacable for
														  testability. */
};

/**
 * @brief A data structure representing a simple linked list of connections.
 */
struct llp_connection_list_node {
	struct llp_connection_list_node *next;
	struct firefly_connection *conn;
};

/**
 * @brief Compares a connection to some value. This function is used by
 * find_connection to test if a connection is the one searched for.
 *
 * @param conn The connection to compare.
 * @param context The data to compare the connection to.
 * @retval true if the context is considered identifying the connection.
 * @retval false otherwise.
 */
typedef bool (*conn_eq_f)(struct firefly_connection *conn, void *context);

/**
 * @brief Adds a connection to the connection list in \a llp.
 *
 * @param conn The connection to add.
 * @param llp The link layer port structure to add the connection to.
 */
void add_connection_to_llp(struct firefly_connection *conn,
		struct firefly_transport_llp *llp);

/**
 * @brief Removes the first \c struct #firefly_connection for which the equals
 * function returns true.
 *
 * @param llp The llp the remove a connection from.
 * @param context The data to compare each connection to with \p conn_eq
 * @param conn_eq The function used to determine if any connection is the
 * connection to remove.
 * @return The removed connection.
 * @retval NULL If the connection was not found.
 * @see find_connection()
 */
struct firefly_connection *remove_connection_from_llp(
		struct firefly_transport_llp *llp,
		void *context, conn_eq_f conn_eq);

/**
 * @brief Finds the first \c struct #firefly_connection, associated with the
 * given \a llp, for which the equals function returns true.
 *
 * @param llp The link layer port to search for the #firefly_connection in.
 * @param context The context passed to the equals function.
 * @param conn_eq The function used to determine if any connection is the
 * connection searched for.
 * @return The #firefly_connection which is found.
 * @retval NULL is returned if no connection was found.
 */
struct firefly_connection *find_connection(struct firefly_transport_llp *llp,
		void *context, conn_eq_f conn_eq);

/**
 * @brief An implementation of #conn_eq_f which compares the pointer value.
 *
 * @param conn The #firefly_connection to compare to.
 * @param context A pointer to a #firefly_connection.
 * @return Where conn and context is equal, pointer comparison.
 */
bool firefly_connection_eq_ptr(struct firefly_connection *conn, void *context);

/**
 * @brief Replaces the callback called when data is received.
 *
 * Should normally not be used except when testing *only* transport layers.
 */
void replace_protocol_data_received_cb(struct firefly_transport_llp *llp,
		protocol_data_received_f protocol_data_received_cb);

#endif
