
#include <stdbool.h>

#include "CUnit/Basic.h"
#include "CUnit/Console.h"

#include <transport/firefly_transport.h>
#include "transport/firefly_transport_private.h"
#include "protocol/firefly_protocol_private.h"

int init_suit_general()
{
	return 0;
}

int clean_suit_general()
{
	return 0;
}

void test_add_conn_to_llp()
{
	struct firefly_transport_llp *llp = calloc(1,
			sizeof(struct firefly_transport_llp));
	long data_1 = 1234;

	struct firefly_connection *conn_1 = firefly_connection_new(NULL, NULL,
							NULL, NULL, NULL, NULL, &data_1, NULL);
	add_connection_to_llp(conn_1, llp);

	CU_ASSERT_PTR_NOT_NULL(llp->conn_list);
	CU_ASSERT_PTR_NOT_NULL(llp->conn_list->conn);
	CU_ASSERT_PTR_EQUAL(llp->conn_list->conn, conn_1);
	CU_ASSERT_PTR_NULL(llp->conn_list->next);

	long data_2 = 1234;
	struct firefly_connection *conn_2 = firefly_connection_new(NULL, NULL, NULL,
															NULL, NULL, NULL,
															&data_2, NULL);
	add_connection_to_llp(conn_2, llp);

	CU_ASSERT_PTR_NOT_NULL(llp->conn_list);
	CU_ASSERT_PTR_NOT_NULL(llp->conn_list->conn);
	CU_ASSERT_PTR_EQUAL(llp->conn_list->conn, conn_2);
	CU_ASSERT_PTR_NOT_NULL(llp->conn_list->next);
	CU_ASSERT_PTR_EQUAL(llp->conn_list->next->conn, conn_1);

	struct llp_connection_list_node *head = llp->conn_list;
	struct llp_connection_list_node *tmp = NULL;
	while (head != NULL) {
		tmp = head->next;
		firefly_connection_free_event(head->conn);
		free(head);
		head = tmp;
	}
	free(llp);
}

bool conn_eq_test(struct firefly_connection *conn, void *context)
{
	long a = *((long *) conn->transport_conn_platspec);
	long b = *((long *) context);
	return a == b;
}

void test_remove_conn_by_addr()
{
	struct firefly_transport_llp *llp = calloc(1,
			sizeof(struct firefly_transport_llp));

	long data_1 = 1234;
	struct firefly_connection *conn_1 = firefly_connection_new(NULL, NULL,
							NULL, NULL, NULL, NULL, &data_1, NULL);
	CU_ASSERT_PTR_NOT_NULL(conn_1);

	struct firefly_connection *conn = find_connection(llp, &data_1,
			conn_eq_test);
	CU_ASSERT_PTR_NULL(conn);
	// Add connection to conn_list
	struct llp_connection_list_node *node_1 =
		malloc(sizeof(struct llp_connection_list_node));
	node_1->conn = conn_1;
	node_1->next = NULL;
	llp->conn_list = node_1;

	// Add a second connection after the first one
	long data_2 = 2234;
	struct firefly_connection *conn_2 = firefly_connection_new(NULL, NULL,
							NULL, NULL, NULL, NULL, &data_2, NULL);
	struct llp_connection_list_node *node_2 =
		malloc(sizeof(struct llp_connection_list_node));
	node_2->conn = conn_2;
	node_2->next = NULL;
	node_1->next = node_2;

	conn = remove_connection_from_llp(llp, &data_1, conn_eq_test);
	CU_ASSERT_PTR_NOT_NULL(conn);
	CU_ASSERT_PTR_NOT_NULL(llp->conn_list);
	CU_ASSERT_TRUE(conn_eq_test(llp->conn_list->conn, &data_2))
	CU_ASSERT_TRUE(conn_eq_test(conn, &data_1))
	CU_ASSERT_PTR_NULL(llp->conn_list->next)
	firefly_connection_free_event(conn);

	conn = remove_connection_from_llp(llp, &data_2, conn_eq_test);
	CU_ASSERT_PTR_NOT_NULL(conn);
	CU_ASSERT_PTR_NULL(llp->conn_list);
	CU_ASSERT_TRUE(conn_eq_test(conn, &data_2))
	firefly_connection_free_event(conn);

	conn = remove_connection_from_llp(llp, &data_1, conn_eq_test);
	CU_ASSERT_PTR_NULL(conn);
	conn = remove_connection_from_llp(llp, &data_2, conn_eq_test);
	CU_ASSERT_PTR_NULL(conn);

	free(llp);
}

void test_find_conn_by_addr()
{
	struct firefly_transport_llp *llp = calloc(1,
			sizeof(struct firefly_transport_llp));

	long data_1 = 1234;
	struct firefly_connection *conn_1 = firefly_connection_new(NULL, NULL,
							NULL, NULL, NULL, NULL, &data_1, NULL);
	CU_ASSERT_PTR_NOT_NULL(conn_1);

	struct firefly_connection *conn = find_connection(llp, &data_1,
			conn_eq_test);
	CU_ASSERT_PTR_NULL(conn);
	// Add connection to conn_list
	struct llp_connection_list_node *node_1 =
		malloc(sizeof(struct llp_connection_list_node));
	node_1->conn = conn_1;
	node_1->next = NULL;
	llp->conn_list = node_1;
	// Try to find conn after adding it
	conn = find_connection(llp, &data_1, conn_eq_test);
	CU_ASSERT_PTR_NOT_NULL(conn);
	CU_ASSERT_TRUE(conn_eq_test(conn, &data_1));

	// Add a second connection after the first one and try to find it
	long data_2 = 2234;
	struct firefly_connection *conn_2 = firefly_connection_new(NULL, NULL,
							NULL, NULL, NULL, NULL, &data_2, NULL);
	struct llp_connection_list_node *node_2 =
		malloc(sizeof(struct llp_connection_list_node));
	node_2->conn = conn_2;
	node_2->next = NULL;
	node_1->next = node_2;
	conn = find_connection(llp, &data_2, conn_eq_test);
	CU_ASSERT_PTR_NOT_NULL(conn);
	CU_ASSERT_TRUE(conn_eq_test(conn, &data_2));

	struct llp_connection_list_node *head = llp->conn_list;
	struct llp_connection_list_node *tmp = NULL;
	while (head != NULL) {
		tmp = head->next;
		firefly_connection_free_event(head->conn);
		free(head);
		head = tmp;
	}
	free(llp);
}
