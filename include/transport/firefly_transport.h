/**
 * @file
 * @brief General transport realted data structures and functions.
 */

#ifndef FIREFLY_TRANSPORT_H
#define FIREFLY_TRANSPORT_H

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
