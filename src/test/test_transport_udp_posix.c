/**
 * @file
 * @brief Test the transport layer with POSIX UDP.
 */
#include "test/test_transport_udp_posix.h"

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

int init_suit_udp_posix()
{
	return 0; // Success.
}

int clean_suit_udp_posix()
{
	return 0; // Success.
}

static unsigned short local_port = 55555;
static unsigned short remote_port = 55556;
static unsigned char send_buf[] = {0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

static unsigned char send_buf_med[] = {0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
				  0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

static unsigned char send_buf_big[] = {0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
				   0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
				   0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
				   0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

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
	unsigned char recv_buf[sizeof(send_buf)];
	struct sockaddr_in remote_addr;
	socklen_t len = sizeof(struct sockaddr_in);
	int res = recvfrom(remote_socket, recv_buf, sizeof(send_buf), 0,
			(struct sockaddr *) &remote_addr, &len);
	if (res == -1) {
		CU_FAIL("Failed to receive data from socket.\n");
	}
	CU_ASSERT_EQUAL(sizeof(send_buf), res);
	CU_ASSERT_NSTRING_EQUAL(recv_buf, send_buf, sizeof(send_buf));

}

void send_data(struct sockaddr_in *remote_addr, unsigned short port,
			   unsigned char *buf, size_t n)
{
	struct sockaddr_in *si_other = calloc(1, sizeof(struct sockaddr_in));
	setup_sockaddr(si_other, local_port);

	// Setup addr to send from
	setup_sockaddr(remote_addr, port);
	// Init and bind remote socket
	int remote_socket = open_socket(remote_addr);

	// Send data.
	int res = sendto(remote_socket, buf, n, 0,
		       	(struct sockaddr *) si_other, sizeof(*si_other));
	if (res == -1) {
		CU_FAIL("Could not send to local socket.\n");
	}
	close(remote_socket);
	free(si_other);
}

static bool data_received = false;
static size_t data_recv_size;
static unsigned char *data_recv_buf;

void protocol_data_received(struct firefly_connection *conn, unsigned char *data,
							size_t size)
{
	if (!data_recv_buf) {
		data_recv_buf = send_buf;
		data_recv_size = sizeof(send_buf);
	}
	CU_ASSERT_EQUAL(data_recv_size, size);
	CU_ASSERT_NSTRING_EQUAL(data, data_recv_buf, size);
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
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
						local_port, recv_conn_recv_conn);
	struct sockaddr_in remote_addr;

	// send data
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	CU_ASSERT_TRUE(good_conn_received);
	data_received = false;
	good_conn_received = false;
	firefly_transport_udp_posix_free(&llp);
}

bool recv_data_recv_conn(struct firefly_connection *conn)
{
	CU_FAIL("Received connection but shouldn't have.\n");
	return true;
}

void test_recv_data()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_data_recv_conn);
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	struct protocol_connection_udp_posix *conn_udp =
		malloc(sizeof(struct protocol_connection_udp_posix));

	conn_udp->remote_addr = malloc(sizeof(struct sockaddr_in));
	memcpy(conn_udp->remote_addr, &remote_addr, sizeof(remote_addr));
	struct firefly_connection *conn = malloc(sizeof(
						struct firefly_connection));
	conn->transport_conn_platspec = conn_udp;

	add_connection_to_llp(conn, llp);

	firefly_transport_udp_posix_read(llp);
	CU_ASSERT_TRUE(data_received);
	data_received = false;
	good_conn_received = false;
	firefly_transport_udp_posix_free(&llp);
}

void test_recv_conn_and_data()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_conn_recv_conn);
	struct sockaddr_in remote_addr;

	// send data
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;
	firefly_transport_udp_posix_free(&llp);
}

void test_recv_conn_and_two_data()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_conn_recv_conn);
	struct sockaddr_in remote_addr;

	// send data
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;

	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));
	firefly_transport_udp_posix_read(llp);

	CU_ASSERT_FALSE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;
	firefly_transport_udp_posix_free(&llp);
}

void test_recv_conn_keep()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_conn_recv_conn);
	// send data
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	struct firefly_connection *conn = find_connection_by_addr(&remote_addr,
									llp);
	CU_ASSERT_NOT_EQUAL(conn, NULL);

	good_conn_received = false;
	data_received = false;
	firefly_transport_udp_posix_free(&llp);
}

bool recv_conn_keep_two(struct firefly_connection *conn)
{
	good_conn_received = true;
	return true;
}

void test_recv_conn_keep_two()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
						local_port, recv_conn_keep_two);
	// send data
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);

	// test first connection
	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	struct firefly_connection *conn = find_connection_by_addr(&remote_addr,
									llp);
	CU_ASSERT_NOT_EQUAL(conn, NULL);

	good_conn_received = false;
	data_received = false;

	send_data(&remote_addr, 55558, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);

	// test first connection
	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	conn = find_connection_by_addr(&remote_addr, llp);
	CU_ASSERT_NOT_EQUAL(conn, NULL);

	good_conn_received = false;
	data_received = false;
	firefly_transport_udp_posix_free(&llp);
}

bool recv_conn_reject_recv_conn(struct firefly_connection *conn)
{
	good_conn_received = true;
	return false;
}

void test_recv_conn_reject()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_conn_reject_recv_conn);
	// send data
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_FALSE(data_received);

	CU_ASSERT_EQUAL(llp->conn_list, NULL);
	firefly_transport_udp_posix_free(&llp);
}

// NOTE: This test is supposed to segfault if it fails as that is the only way
// to test it.
void test_null_pointer_as_callback()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
							local_port, NULL);
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	firefly_transport_udp_posix_read(llp);

	CU_PASS("Passed null pointer as callback\n");

	firefly_transport_udp_posix_free(&llp);
}

void test_find_conn_by_addr()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
							local_port, NULL);
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

	firefly_transport_udp_posix_free(&llp);
}

void test_add_conn_to_llp()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
							local_port, NULL);
	struct sockaddr_in addr_1;
	setup_sockaddr(&addr_1, 55550);

	struct protocol_connection_udp_posix *conn_udp_1 =
		malloc(sizeof(struct protocol_connection_udp_posix));

	conn_udp_1->remote_addr = malloc(sizeof(struct sockaddr_in));
	memcpy(conn_udp_1->remote_addr, &addr_1, sizeof(addr_1));
	struct firefly_connection *conn_1 = malloc(sizeof(
						struct firefly_connection));
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
	struct firefly_connection *conn_2 = malloc(sizeof(
						struct firefly_connection));
	conn_2->transport_conn_platspec = conn_udp_2;
	add_connection_to_llp(conn_2, llp);

	CU_ASSERT_PTR_NOT_NULL(llp->conn_list);
	CU_ASSERT_PTR_NOT_NULL(llp->conn_list->conn);
	CU_ASSERT_PTR_EQUAL(llp->conn_list->conn, conn_2);
	CU_ASSERT_PTR_NOT_NULL(llp->conn_list->next);
	CU_ASSERT_PTR_EQUAL(llp->conn_list->next->conn, conn_1);

	firefly_transport_udp_posix_free(&llp);
}

void test_conn_open_and_send()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
							local_port, NULL);
	struct firefly_connection *conn = 
		firefly_transport_connection_udp_posix_open("127.0.0.1", 55550,
								llp);
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	struct sockaddr_in recv_addr;
	setup_sockaddr(&recv_addr, 55550);
	int recv_soc = open_socket(&recv_addr);

	firefly_transport_write_udp_posix(send_buf, sizeof(send_buf), conn);

	recv_data(recv_soc);

	close(recv_soc);
	firefly_transport_udp_posix_free(&llp);
}

void test_conn_open_and_recv()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_data_recv_conn);

	struct firefly_connection *conn =
		firefly_transport_connection_udp_posix_open("127.0.0.1", 55550,
								llp);
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	struct sockaddr_in send_addr;
	send_data(&send_addr, 55550, send_buf, sizeof(send_buf));

	firefly_transport_udp_posix_read(llp);

	CU_ASSERT_TRUE(data_received);

	data_received = false;
	firefly_transport_udp_posix_free(&llp);
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
	struct firefly_transport_llp *llp_recv = 
		firefly_transport_llp_udp_posix_new(local_port,
			       	open_and_recv_conn_recv_conn);

	struct firefly_transport_llp *llp_send =
		firefly_transport_llp_udp_posix_new(remote_port,
					recv_data_recv_conn);

	struct firefly_connection *conn_send = 
		firefly_transport_connection_udp_posix_open("127.0.0.1",
							local_port, llp_send);
	CU_ASSERT_PTR_NOT_NULL(conn_send);

	firefly_transport_write_udp_posix(send_buf, sizeof(send_buf),
						conn_send);

	firefly_transport_udp_posix_read(llp_recv);

	CU_ASSERT_PTR_NOT_NULL(conn_recv);
	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;

	firefly_transport_write_udp_posix(send_buf, sizeof(send_buf),
						conn_recv);

	firefly_transport_udp_posix_read(llp_send);
	CU_ASSERT_TRUE(data_received);

	data_received = false;
	firefly_transport_udp_posix_free(&llp_recv);
	firefly_transport_udp_posix_free(&llp_send);
}

void test_recv_big_connection()
{
	struct firefly_transport_llp *llp =
		firefly_transport_llp_udp_posix_new(local_port,
							recv_conn_recv_conn);
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	data_received = false;		/* test globals */
	good_conn_received = false;	/* ditto ... */
	data_recv_size = sizeof(send_buf);
	data_recv_buf = send_buf;

	firefly_transport_udp_posix_read(llp);
	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);

	send_data(&remote_addr, remote_port, send_buf_big,
			sizeof(send_buf_big));

	data_received = false;		/* same test globals ... */
	good_conn_received = false;
	data_recv_size = sizeof(send_buf_big);
	data_recv_buf = send_buf_big;

	firefly_transport_udp_posix_read(llp);
	CU_ASSERT_TRUE(data_received);

	data_received = false;
	/* good_conn_received = false; */
	firefly_transport_udp_posix_free(&llp);
}

void test_reader_scale_back()
{
	struct sockaddr_in remote_addr;
	struct firefly_transport_llp *llp;
	const unsigned int sbn = 8;

	data_recv_size = sizeof(send_buf_big); /* test realted global */
	data_recv_buf = send_buf_big;			   /* ditto */

	llp = firefly_transport_llp_udp_posix_new(local_port,
							recv_conn_recv_conn);
	firefly_transport_udp_posix_set_n_scaleback(llp, sbn);

	send_data(&remote_addr, remote_port,
			  send_buf_big, sizeof(send_buf_big));
	firefly_transport_udp_posix_read(llp);
	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;

	for (int i = 0; i <= sbn; i++) {
		unsigned char *buf;
		unsigned int s;
		if (i != 3) {
			buf = send_buf;
			s = sizeof(send_buf);
			data_recv_size = sizeof(send_buf);
		} else {
			buf = send_buf_med;
			s = sizeof(send_buf_med);
			data_recv_size = sizeof(send_buf_med);
		}
		send_data(&remote_addr, remote_port, buf, s);
		data_recv_buf = buf;
		firefly_transport_udp_posix_read(llp);
		CU_ASSERT_TRUE(data_received);
		data_received = false;
	}
	struct transport_llp_udp_posix *udp_llp =
		(struct transport_llp_udp_posix *)llp->llp_platspec;

	CU_ASSERT_EQUAL(udp_llp->recv_buf_size, sizeof(send_buf_med));
	firefly_transport_udp_posix_free(&llp);
	data_received = false;
	good_conn_received = false;
}
