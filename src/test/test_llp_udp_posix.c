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

#include <transport/firefly_transport_udp_posix.h>
#include <protocol/firefly_protocol.h>
#include "transport/firefly_transport_udp_posix_private.h"

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
static unsigned char send_buf[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

void send_data(struct sockaddr_in *remote_addr, unsigned short port) {
	struct sockaddr_in *si_other = calloc(1, sizeof(struct sockaddr_in));
	si_other->sin_family = AF_INET;
	si_other->sin_port = htons(local_port);
	if (inet_pton(AF_INET, "127.0.0.1", &si_other->sin_addr) == 0) {
		CU_FAIL("Failed to convert string to network IP.\n");
	}

	// Init and bind remote socket
	int remote_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (remote_socket == -1) {
		CU_FAIL("Failed to open socket.\n");
	}
	remote_addr->sin_family = AF_INET;
	remote_addr->sin_port = htons(port);
	if (inet_pton(AF_INET, "127.0.0.1", &remote_addr->sin_addr) == 0) {
		CU_FAIL("Failed to convert string to network IP.\n");
	}
	int res = bind(remote_socket, (struct sockaddr *) remote_addr,
		       	sizeof(struct sockaddr_in));
	if (res == -1) {
		CU_FAIL("Failed to bind remote socket.\n");
	}

	// Send data.
	res = sendto(remote_socket, send_buf, SEND_BUF_SIZE, 0,
		       	(struct sockaddr *) si_other, sizeof(*si_other));
	if (res == -1) {
		CU_FAIL("Could not send to local socket.\n");
	}
	close(remote_socket);
	free(si_other);
}

static bool data_received = false;

void protocol_data_received(struct connection *conn, unsigned char *data,
		size_t size)
{
	CU_ASSERT_EQUAL(SEND_BUF_SIZE, size);
	CU_ASSERT_NSTRING_EQUAL(data, send_buf, size);
	data_received = true;
}

static bool good_conn_received = false;
/* Callback when a new connection arrives at transport layer. */
bool recv_conn_recv_conn(struct connection *conn)
{
	struct protocol_connection_udp_posix *pcup =
		(struct protocol_connection_udp_posix *)
		conn->transport_conn_platspec;

	char ipaddr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &pcup->remote_addr->sin_addr, ipaddr, INET_ADDRSTRLEN);
	CU_ASSERT_STRING_EQUAL(ipaddr, "127.0.0.1");
	CU_ASSERT_EQUAL(ntohs(pcup->remote_addr->sin_port), remote_port);

	return good_conn_received = true;
}

/* Test recieving a new connection.*/
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

bool recv_data_recv_conn(struct connection *conn)
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
	struct connection *conn = malloc(sizeof(struct connection));
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

	struct connection *conn = find_connection_by_addr(&remote_addr, llp);
	CU_ASSERT_NOT_EQUAL(conn, NULL);

	good_conn_received = false;
	data_received = false;
	transport_llp_udp_posix_free(&llp);
}

bool recv_conn_keep_two(struct connection *conn)
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

	struct connection *conn = find_connection_by_addr(&remote_addr, llp);
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

bool recv_conn_reject_recv_conn(struct connection *conn)
{
	struct protocol_connection_udp_posix *pcup =
		(struct protocol_connection_udp_posix *)
		conn->transport_conn_platspec;

	char ipaddr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &pcup->remote_addr->sin_addr, ipaddr, INET_ADDRSTRLEN);
	CU_ASSERT_STRING_EQUAL(ipaddr, "127.0.0.1");
	CU_ASSERT_EQUAL(ntohs(pcup->remote_addr->sin_port), remote_port);

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
		(CU_add_test(pSuite, "test_recv_connection", test_recv_connection) == NULL)
		       ||
		(CU_add_test(pSuite, "test_recv_data", test_recv_data) == NULL)
		       ||
		(CU_add_test(pSuite, "test_recv_conn_and_data", test_recv_conn_and_data) == NULL)
		       ||
		(CU_add_test(pSuite, "test_recv_conn_keep", test_recv_conn_keep) == NULL)
		       ||
		(CU_add_test(pSuite, "test_recv_conn_reject", test_recv_conn_reject) == NULL)
		       ||
		(CU_add_test(pSuite, "test_recv_conn_keep_two", test_recv_conn_keep_two) == NULL)
		       ||
		(CU_add_test(pSuite, "test_null_pointer_as_callback", test_null_pointer_as_callback) == NULL)
		/*(CU_add_test(pSuite, "test_2", test_2) == NULL)*/
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
