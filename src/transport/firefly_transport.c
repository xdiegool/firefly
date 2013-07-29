/**
 * @file
 * @brief Common functions for transport.
 */
#include "transport/firefly_transport_private.h"

#include <stdbool.h>

#include <transport/firefly_transport.h>

#include "protocol/firefly_protocol_private.h"
#include "utils/firefly_errors.h"

void add_connection_to_llp(struct firefly_connection *conn,
		struct firefly_transport_llp *llp)
{
	struct llp_connection_list_node *tmp;
	struct llp_connection_list_node *new_node;

	tmp = llp->conn_list;
	new_node = FIREFLY_MALLOC(sizeof(*new_node));
	if (!new_node) {
		FFL(FIREFLY_ERROR_ALLOC);
		return;
	}
	new_node->conn = conn;
	new_node->next = tmp;
	llp->conn_list = new_node;
}

struct firefly_connection *remove_connection_from_llp(
		struct firefly_transport_llp *llp,
		void *context, conn_eq_f conn_eq)
{
	struct firefly_connection *ret = NULL;
	struct llp_connection_list_node **head = &llp->conn_list;
	/*printf("head: %p\n", head);*/
	while (head != NULL && (*head) != NULL) {
		if (conn_eq((*head)->conn, context)) {
			ret = (*head)->conn;
			struct llp_connection_list_node *tmp = (*head)->next;
			FIREFLY_FREE(*head);
			*head = tmp;
			head = NULL;
		} else {
			*head = (*head)->next;
		}
	}
	return ret;
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

bool firefly_connection_eq_ptr(struct firefly_connection *conn, void *context)
{
	return conn == context;
}

void replace_protocol_data_received_cb(struct firefly_transport_llp *llp,
		protocol_data_received_f protocol_data_received_cb)
{
	llp->protocol_data_received_cb = protocol_data_received_cb;
}
