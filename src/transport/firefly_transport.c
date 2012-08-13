/**
 * @file
 * @brief Common functions for transport.
 */
#include "transport/firefly_transport_private.h"

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
