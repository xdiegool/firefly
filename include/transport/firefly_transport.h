/**
 * @file
 * @brief General transport realted data structures and functions.
 */

#ifndef FIREFLY_TRANSPORT_H
#define FIREFLY_TRANSPORT_H

/**
 * @brief This callback which will be called when a new connection is received.
 *
 * This function is implemented by the application layer. It will be called
 * when the transport layer receives a new connection. The return value of this
 * function specifies whether to accept the connection or refuse it, true will
 * accept it and false will refuse it.
 * @param conn The received connection
 * @return Return true to accept the connection and false to refuse it
 */
typedef bool (*application_on_conn_recv_cb)(struct connection *conn);

struct llp_connection_list_node {
	struct llp_connection_list_node *next;
	struct connection *conn;
};

struct transport_llp {
	application_on_conn_recv_cb on_conn_recv;
	struct llp_connection_list_node *conn_list;
	void *llp_platspec; // Platform specific data.
};	

#endif
