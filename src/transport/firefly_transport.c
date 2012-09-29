/**
 * @file
 * @brief Common functions for transport.
 */
#include "transport/firefly_transport_private.h"

#include <stdbool.h>

#include <transport/firefly_transport.h>

void add_connection_to_llp(struct firefly_connection *conn,
		struct firefly_transport_llp *llp)
{
	struct llp_connection_list_node *tmp = llp->conn_list;
	struct llp_connection_list_node *new_node =
		malloc(sizeof(struct llp_connection_list_node));
	new_node->conn = conn;
	new_node->next = tmp;
	llp->conn_list = new_node;
}

struct firefly_connection *find_connection(struct firefly_transport_llp *llp,
		void *context, conn_eq_f conn_eq)
{
	struct llp_connection_list_node *head = llp->conn_list;
	struct firefly_connection *conn;

	while (head != NULL) {
		conn = head->conn;
		if (conn_eq(conn, context)) {
			return head->conn;
		} else {
			head = head->next;
		}
	}
	return NULL;
}
