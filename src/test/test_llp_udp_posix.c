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

#include <transport/firefly_transport_udp_posix.h>
#include <protocol/firefly_protocol.h>

int init_suit()
{
	printf("Initializing suit.\n");
	return 0; // Success.
}

int clean_suit()
{
	printf("Cleaning suit.\n");
	return 0; // Success.
}

static bool good_conn_received = false;
static unsigned short local_port = 1025;
static unsigned short remote_port = 1024;

/* Callback when a new connection arrives at transport layer. */
bool app_connection_new(struct connection *conn)
{
	struct protocol_connection_udp_posix *pcup =
	       	(struct protocol_connection_udp_posix *)
	       	conn->transport_conn_platspec;

	char ipaddr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &pcup->remote_addr, ipaddr, INET_ADDRSTRLEN);
	CU_ASSERT_STRING_EQUAL(ipaddr, "127.0.0.1");

	CU_ASSERT_EQUAL(pcup->remote_addr->sin_port, remote_port);

	return good_conn_received = true;
}

/* Test recieving a new connection.*/
void test_recv_connection()
{
	struct transport_llp *llp = transport_llp_udp_posix_new(local_port,
		       	app_connection_new);

	// Set up a connection over local loopback.
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
	struct sockaddr_in *remote_addr = calloc(1, sizeof(struct sockaddr_in));
	remote_addr->sin_family = AF_INET;
	remote_addr->sin_port = htons(remote_port);
	remote_addr->sin_addr.s_addr = htonl(INADDR_ANY);
	int res = bind(remote_socket, (struct sockaddr *) remote_addr,
		       	sizeof(struct sockaddr_in));
	if (res == -1) {
		CU_FAIL("Failed to bind remote socket.\n");
	}

	// Send data.
	unsigned char send_buf[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
	res = sendto(remote_socket, send_buf, 16, 0,
		       	(struct sockaddr *) si_other, sizeof(*si_other));
	if (res == -1) {
		CU_FAIL("Could not send to local socket.\n");
	}
	transport_llp_udp_posix_read(llp);
	CU_ASSERT_TRUE(good_conn_received);
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
		       /*||*/
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
