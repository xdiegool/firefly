/**
 * @file
 * @brief Test the transport layer with POSIX UDP.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_udp_posix.h>

#include "transport/firefly_transport_private.h"
#include "transport/firefly_transport_udp_posix_private.h"
#include "protocol/firefly_protocol_private.h"

#define SEND_BUF_SIZE	(16)

int init_suit()
{
	//printf("Initializing suit.\n");
	return 0; // Success.
}

int clean_suit()
{
	//printf("Cleaning suit.\n");
	return 0; // Success.
}

static unsigned short local_port = 55555;
static unsigned short remote_port = 55556;
static unsigned char send_buf[] = {0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

void setup_sockaddr(struct sockaddr_in *addr, unsigned short port)
{
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	if (inet_pton(AF_INET, "127.0.0.1", &addr->sin_addr) == 0) {
		CU_FAIL("Failed to convert string to network IP.\n");
	}
}

int open_socket(struct sockaddr_in *addr)
{
	int remote_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (remote_socket == -1) {
		CU_FAIL("Failed to open socket.\n");
	}
	int res = bind(remote_socket, (struct sockaddr *) addr,
		       	sizeof(struct sockaddr_in));
	if (res == -1) {
		CU_FAIL("Failed to bind remote socket.\n");
	}
	return remote_socket;
}

void recv_data(int remote_socket)
{
	unsigned char recv_buf[SEND_BUF_SIZE];
	struct sockaddr_in remote_addr;
	socklen_t len = sizeof(struct sockaddr_in);
	int res = recvfrom(remote_socket, recv_buf, SEND_BUF_SIZE, 0,
			(struct sockaddr *) &remote_addr, &len);
	if (res == -1) {
		CU_FAIL("Failed to receive data from socket.\n");
	}
	CU_ASSERT_EQUAL(SEND_BUF_SIZE, res);
	CU_ASSERT_NSTRING_EQUAL(recv_buf, send_buf, SEND_BUF_SIZE);

}

void send_data(struct sockaddr_in *remote_addr, unsigned short port)
{
	struct sockaddr_in *si_other = calloc(1, sizeof(struct sockaddr_in));
	setup_sockaddr(si_other, local_port);

	// Setup addr to send from
	setup_sockaddr(remote_addr, port);
	// Init and bind remote socket
	int remote_socket = open_socket(remote_addr);

	// Send data.
	int res = sendto(remote_socket, send_buf, SEND_BUF_SIZE, 0,
		       	(struct sockaddr *) si_other, sizeof(*si_other));
	if (res == -1) {
		CU_FAIL("Could not send to local socket.\n");
	}
	close(remote_socket);
	free(si_other);
}

static bool data_received = false;

void protocol_data_received(struct firefly_connection *conn, unsigned char *data,
		size_t size)
{
	CU_ASSERT_EQUAL(SEND_BUF_SIZE, size);
	CU_ASSERT_NSTRING_EQUAL(data, send_buf, size);
	data_received = true;
}

static bool good_conn_received = false;
/* Callback when a new connection arrives at transport layer. */
bool recv_conn_recv_conn(struct firefly_connection *conn)
{
	struct protocol_connection_udp_posix *pcup =
		(struct protocol_connection_udp_posix *)
		conn->transport_conn_platspec;

	char ipaddr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &pcup->remote_addr->sin_addr, ipaddr,
			INET_ADDRSTRLEN);
	CU_ASSERT_STRING_EQUAL(ipaddr, "127.0.0.1");
	CU_ASSERT_EQUAL(ntohs(pcup->remote_addr->sin_port), remote_port);

	return good_conn_received = true;
}

/* Test receiving a new connection.*/
void test_recv_connection()
{
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
			recv_conn_recv_conn);
	struct sockaddr_in remote_addr;

	// send data
	send_data(&remote_addr, remote_port);

	// Set up a connection over local loopback.
	transport_llp_udp_posix_read(llp);
	CU_ASSERT_TRUE(good_conn_received);
	data_received = false;
	good_conn_received = false;
	transport_llp_udp_posix_free(&llp);
}

bool recv_data_recv_conn(struct firefly_connection *conn)
{
	CU_FAIL("Received connection but shouldn't have.\n");
	return true;
}

void test_recv_data() {
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
			recv_data_recv_conn);
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port);

	struct protocol_connection_udp_posix *conn_udp =
		malloc(sizeof(struct protocol_connection_udp_posix));

	conn_udp->remote_addr = malloc(sizeof(struct sockaddr_in));
	memcpy(conn_udp->remote_addr, &remote_addr, sizeof(remote_addr));
	struct firefly_connection *conn = malloc(sizeof(struct firefly_connection));
	conn->transport_conn_platspec = conn_udp;

	add_connection_to_llp(conn, llp);

	transport_llp_udp_posix_read(llp);
	CU_ASSERT_TRUE(data_received);
	data_received = false;
	good_conn_received = false;
	transport_llp_udp_posix_free(&llp);
}

void test_recv_conn_and_data()
{
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
			recv_conn_recv_conn);
	struct sockaddr_in remote_addr;

	// send data
	send_data(&remote_addr, remote_port);

	// Set up a connection over local loopback.
	transport_llp_udp_posix_read(llp);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;
	transport_llp_udp_posix_free(&llp);
}

void test_recv_conn_and_two_data()
{
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
			recv_conn_recv_conn);
	struct sockaddr_in remote_addr;

	// send data
	send_data(&remote_addr, remote_port);

	// Set up a connection over local loopback.
	transport_llp_udp_posix_read(llp);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;

	send_data(&remote_addr, remote_port);
	transport_llp_udp_posix_read(llp);

	CU_ASSERT_FALSE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;
	transport_llp_udp_posix_free(&llp);
}

void test_recv_conn_keep()
{
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
			recv_conn_recv_conn);
	// send data
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port);

	// Set up a connection over local loopback.
	transport_llp_udp_posix_read(llp);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	struct firefly_connection *conn = find_connection_by_addr(&remote_addr, llp);
	CU_ASSERT_NOT_EQUAL(conn, NULL);

	good_conn_received = false;
	data_received = false;
	transport_llp_udp_posix_free(&llp);
}

bool recv_conn_keep_two(struct firefly_connection *conn)
{
	good_conn_received = true;
	return true;
}

void test_recv_conn_keep_two()
{
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
			recv_conn_keep_two);
	// send data
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port);

	// Set up a connection over local loopback.
	transport_llp_udp_posix_read(llp);

	// test first connection
	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	struct firefly_connection *conn = find_connection_by_addr(&remote_addr, llp);
	CU_ASSERT_NOT_EQUAL(conn, NULL);

	good_conn_received = false;
	data_received = false;

	send_data(&remote_addr, 55558);

	// Set up a connection over local loopback.
	transport_llp_udp_posix_read(llp);

	// test first connection
	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	conn = find_connection_by_addr(&remote_addr, llp);
	CU_ASSERT_NOT_EQUAL(conn, NULL);

	good_conn_received = false;
	data_received = false;
	transport_llp_udp_posix_free(&llp);
}

bool recv_conn_reject_recv_conn(struct firefly_connection *conn)
{
	good_conn_received = true;
	return false;
}

void test_recv_conn_reject()
{
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
			recv_conn_reject_recv_conn);
	// send data
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port);

	// Set up a connection over local loopback.
	transport_llp_udp_posix_read(llp);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_FALSE(data_received);

	CU_ASSERT_EQUAL(llp->conn_list, NULL);
	transport_llp_udp_posix_free(&llp);
}

// NOTE: This test is supposed to segfault if it fails as that is the only way
// to test it.
void test_null_pointer_as_callback()
{
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
			NULL);
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port);

	transport_llp_udp_posix_read(llp);

	CU_PASS("Passed null pointer as callback\n");

	transport_llp_udp_posix_free(&llp);
}

void test_find_conn_by_addr()
{
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
			NULL);
	struct sockaddr_in addr_1;
	setup_sockaddr(&addr_1, 55550);
	struct firefly_connection *conn = find_connection_by_addr(&addr_1, llp);
	CU_ASSERT_PTR_NULL(conn);
	// Add connection to conn_list
	struct llp_connection_list_node *node_1 =
		malloc(sizeof(struct llp_connection_list_node));
	struct protocol_connection_udp_posix *conn_udp_1 =
		malloc(sizeof(struct protocol_connection_udp_posix));
	conn_udp_1->remote_addr = malloc(sizeof(struct sockaddr_in));
	memcpy(conn_udp_1->remote_addr, &addr_1, sizeof(addr_1));
	node_1->conn = malloc(sizeof(struct firefly_connection));
	node_1->conn->transport_conn_platspec = conn_udp_1;
	node_1->next = NULL;
	llp->conn_list = node_1;
	// Try to find conn after adding it
	conn = find_connection_by_addr(&addr_1, llp);
	CU_ASSERT_PTR_NOT_NULL(conn);
	CU_ASSERT_TRUE(sockaddr_in_eq(&addr_1,
		((struct protocol_connection_udp_posix *)
		conn->transport_conn_platspec)->remote_addr));

	// Add a second connection after the first one and try to find it
	struct sockaddr_in addr_2;
	setup_sockaddr(&addr_2, 55551);
	struct llp_connection_list_node *node_2 =
		malloc(sizeof(struct llp_connection_list_node));
	struct protocol_connection_udp_posix *conn_udp_2 =
		malloc(sizeof(struct protocol_connection_udp_posix));
	conn_udp_2->remote_addr = malloc(sizeof(struct sockaddr_in));
	memcpy(conn_udp_2->remote_addr, &addr_2, sizeof(addr_2));
	node_2->conn = malloc(sizeof(struct firefly_connection));
	node_2->conn->transport_conn_platspec = conn_udp_2;
	node_2->next = NULL;
	node_1->next = node_2;
	conn = find_connection_by_addr(&addr_2, llp);
	CU_ASSERT_PTR_NOT_NULL(conn);
	CU_ASSERT_TRUE(sockaddr_in_eq(&addr_2,
		((struct protocol_connection_udp_posix *)
		conn->transport_conn_platspec)->remote_addr));

	transport_llp_udp_posix_free(&llp);
}

void test_add_conn_to_llp()
{
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
			NULL);
	struct sockaddr_in addr_1;
	setup_sockaddr(&addr_1, 55550);

	struct protocol_connection_udp_posix *conn_udp_1 =
		malloc(sizeof(struct protocol_connection_udp_posix));

	conn_udp_1->remote_addr = malloc(sizeof(struct sockaddr_in));
	memcpy(conn_udp_1->remote_addr, &addr_1, sizeof(addr_1));
	struct firefly_connection *conn_1 = malloc(sizeof(struct firefly_connection));
	conn_1->transport_conn_platspec = conn_udp_1;
	add_connection_to_llp(conn_1, llp);

	CU_ASSERT_PTR_NOT_NULL(llp->conn_list);
	CU_ASSERT_PTR_NOT_NULL(llp->conn_list->conn);
	CU_ASSERT_PTR_EQUAL(llp->conn_list->conn, conn_1);
	CU_ASSERT_PTR_NULL(llp->conn_list->next);

	struct sockaddr_in addr_2;
	setup_sockaddr(&addr_2, 55550);

	struct protocol_connection_udp_posix *conn_udp_2 =
		malloc(sizeof(struct protocol_connection_udp_posix));

	conn_udp_2->remote_addr = malloc(sizeof(struct sockaddr_in));
	memcpy(conn_udp_2->remote_addr, &addr_2, sizeof(addr_2));
	struct firefly_connection *conn_2 = malloc(sizeof(struct firefly_connection));
	conn_2->transport_conn_platspec = conn_udp_2;
	add_connection_to_llp(conn_2, llp);

	CU_ASSERT_PTR_NOT_NULL(llp->conn_list);
	CU_ASSERT_PTR_NOT_NULL(llp->conn_list->conn);
	CU_ASSERT_PTR_EQUAL(llp->conn_list->conn, conn_2);
	CU_ASSERT_PTR_NOT_NULL(llp->conn_list->next);
	CU_ASSERT_PTR_EQUAL(llp->conn_list->next->conn, conn_1);

	transport_llp_udp_posix_free(&llp);
}

void test_conn_open_and_send()
{
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
			NULL);
	struct firefly_connection *conn = transport_connection_udp_posix_open(
			"127.0.0.1", 55550, llp);
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	struct sockaddr_in recv_addr;
	setup_sockaddr(&recv_addr, 55550);
	int recv_soc = open_socket(&recv_addr);

	transport_write_udp_posix(send_buf, SEND_BUF_SIZE, conn);

	recv_data(recv_soc);

	close(recv_soc);
	transport_llp_udp_posix_free(&llp);
}

void test_conn_open_and_recv()
{
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
			recv_data_recv_conn);

	struct firefly_connection *conn = transport_connection_udp_posix_open(
			"127.0.0.1", 55550, llp);
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	struct sockaddr_in send_addr;
	send_data(&send_addr, 55550);

	transport_llp_udp_posix_read(llp);

	CU_ASSERT_TRUE(data_received);

	data_received = false;
	transport_llp_udp_posix_free(&llp);
}

static struct firefly_connection *conn_recv = NULL;
/* Callback when a new connection arrives at transport layer. */
bool open_and_recv_conn_recv_conn(struct firefly_connection *conn)
{
	conn_recv = conn;
	return good_conn_received = true;
}

void test_open_and_recv_with_two_llp()
{
	struct transport_llp *llp_recv = transport_llp_udp_posix_new(local_port,
			open_and_recv_conn_recv_conn);

	struct transport_llp *llp_send =
		transport_llp_udp_posix_new(remote_port, recv_data_recv_conn);

	struct firefly_connection *conn_send = transport_connection_udp_posix_open(
			"127.0.0.1", local_port, llp_send);
	CU_ASSERT_PTR_NOT_NULL(conn_send);

	transport_write_udp_posix(send_buf, SEND_BUF_SIZE, conn_send);

	transport_llp_udp_posix_read(llp_recv);

	CU_ASSERT_PTR_NOT_NULL(conn_recv);
	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;

	transport_write_udp_posix(send_buf, SEND_BUF_SIZE, conn_recv);

	transport_llp_udp_posix_read(llp_send);
	CU_ASSERT_TRUE(data_received);

	data_received = false;
	transport_llp_udp_posix_free(&llp_recv);
	transport_llp_udp_posix_free(&llp_send);
}

int main()
{
	CU_pSuite pSuite = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	// Add our test suites.
	pSuite = CU_add_suite("udp_core", init_suit, clean_suit);
	if (pSuite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	
	if (
		(CU_add_test(pSuite, "test_find_conn_by_addr",
				test_find_conn_by_addr) == NULL)
		       ||
		(CU_add_test(pSuite, "test_add_conn_to_llp",
				test_add_conn_to_llp) == NULL)
		       ||
		(CU_add_test(pSuite, "test_recv_connection",
				test_recv_connection) == NULL)
		       ||
		(CU_add_test(pSuite, "test_recv_data",
				test_recv_data) == NULL)
		       ||
		(CU_add_test(pSuite, "test_recv_conn_and_data",
				test_recv_conn_and_data) == NULL)
		       ||
		(CU_add_test(pSuite, "test_recv_conn_keep",
				test_recv_conn_keep) == NULL)
		       ||
		(CU_add_test(pSuite, "test_recv_conn_reject",
				test_recv_conn_reject) == NULL)
		       ||
		(CU_add_test(pSuite, "test_recv_conn_keep_two",
				test_recv_conn_keep_two) == NULL)
		       ||
		(CU_add_test(pSuite, "test_recv_conn_and_two_data",
				test_recv_conn_and_two_data) == NULL)
		       ||
		(CU_add_test(pSuite, "test_null_pointer_as_callback",
				test_null_pointer_as_callback) == NULL)
		       ||
		(CU_add_test(pSuite, "test_conn_open_and_send",
				test_conn_open_and_send) == NULL)
		       ||
		(CU_add_test(pSuite, "test_conn_open_and_recv",
				test_conn_open_and_recv) == NULL)
		       ||
		(CU_add_test(pSuite, "test_open_and_recv_with_two_llp",
				test_open_and_recv_with_two_llp) == NULL)
	   ) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Set verbosity.
	CU_basic_set_mode(CU_BRM_VERBOSE);
	/*CU_console_run_tests();*/

	// Run all test suites.
	CU_basic_run_tests();

	// Clean up.
	CU_cleanup_registry();

	return CU_get_error();
}
