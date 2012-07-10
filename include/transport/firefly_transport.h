/**
 * @file
 * @brief General transport realted data structures and functions.
 */

#ifndef FIREFLY_TRANSPORT_H
#define FIREFLY_TRANSPORT_H

typedef bool (*application_on_conn_recv_cb)(struct connection *conn);

struct transport_llp {
	application_on_conn_recv_cb on_conn_recv;
	void *llp_platspec; // Platform specific data.
};	

#endif
