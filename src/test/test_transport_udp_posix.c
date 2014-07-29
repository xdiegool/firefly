/**
 * @file
 * @brief Test the transport layer with POSIX UDP.
 */
#define _POSIX_C_SOURCE (200112L)
#include <pthread.h>
#include "test/test_transport_udp_posix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_udp_posix.h>

#include <utils/firefly_errors.h>
#include "transport/firefly_transport_private.h"
#include "transport/firefly_transport_udp_posix_private.h"
#include "protocol/firefly_protocol_private.h"
#include "test/error_helper.h"
#include "test/event_helper.h"
#include "utils/cppmacros.h"
#include "test_transport.h"

extern unsigned char send_buf[16];
extern bool data_received;
extern size_t data_recv_size;
extern unsigned char *data_recv_buf;

extern bool was_in_error;
extern enum firefly_error expected_error;
static struct firefly_event_queue *eq = NULL;

extern unsigned int nbr_added_events;
extern int64_t test_event_ids[50];
extern int64_t test_event_deps[50][FIREFLY_EVENT_QUEUE_MAX_DEPENDS];

int init_suit_udp_posix()
{
	eq = firefly_event_queue_new(mock_test_event_add, 10, NULL);
	return 0; // Success.
}

int clean_suit_udp_posix()
{
	firefly_event_queue_free(&eq);
	return 0; // Success.
}

static const unsigned short local_port = 55555;
static unsigned short remote_port = 55556;

static unsigned char send_buf_big[] = {0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
				   0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
				   0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
				   0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

static void setup_sockaddr(struct sockaddr_in *addr, unsigned short port)
{
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	if (inet_pton(AF_INET, "127.0.0.1", &addr->sin_addr) == 0) {
		CU_FAIL("Failed to convert string to network IP.\n");
	}
}

static int open_socket(struct sockaddr_in *addr)
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

static void recv_data(int remote_socket)
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

static void send_data(struct sockaddr_in *remote_addr, unsigned short port,
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

static bool good_conn_received = false;
/* Callback when a new connection arrives at transport layer. */
int64_t recv_conn_recv_conn(
		struct firefly_transport_llp *llp, const char *ipaddr, unsigned short port)
{
	CU_ASSERT_STRING_EQUAL(ipaddr, "127.0.0.1");
	CU_ASSERT_EQUAL(port, remote_port);
	good_conn_received = true;
	struct firefly_transport_connection *conn_udp =
		firefly_transport_connection_udp_posix_new(llp, ipaddr, port, 1000);

	int64_t res = firefly_connection_open(NULL, NULL, eq, conn_udp, NULL);
	return res;
}

/* Test receiving a new connection.*/
void test_recv_connection()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
						local_port, recv_conn_recv_conn, eq);
	struct sockaddr_in remote_addr;

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	// send data
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(good_conn_received);
	event_execute_test(eq, 1);
	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
	data_received = false;
	good_conn_received = false;
}

void test_recv_conn_null_cb()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
						local_port, NULL, eq);
	struct sockaddr_in remote_addr;

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	// send data
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	mock_test_event_queue_reset(eq);
	firefly_transport_udp_posix_read(llp);
	CU_ASSERT_EQUAL(nbr_added_events, 1);

	mock_test_event_queue_reset(eq);
	event_execute_test(eq, 1);

	CU_ASSERT_EQUAL(nbr_added_events, 0);
	CU_ASSERT_FALSE(data_received);
	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
	data_received = false;
}

int64_t recv_data_recv_conn(
		struct firefly_transport_llp *llp, const char *ip_addr, unsigned short port)
{
	UNUSED_VAR(llp);
	UNUSED_VAR(ip_addr);
	UNUSED_VAR(port);
	CU_FAIL("Received connection but shouldn't have.\n");
	return 0;
}

static struct firefly_connection *tmp_conn;
static void tmp_on_conn_open(struct firefly_connection *conn, void *ctx)
{
	UNUSED_VAR(ctx);
	tmp_conn = conn;
}

void test_recv_data()
{
	struct firefly_connection *conn;
	struct sockaddr_in remote_addr;
	char ipaddr[INET_ADDRSTRLEN];
	unsigned short port;
	struct firefly_connection_actions actions = {
		.connection_opened = tmp_on_conn_open,
	};
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_data_recv_conn, eq);
	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));
	sockaddr_in_ipaddr(&remote_addr, ipaddr);
	port = sockaddr_in_port(&remote_addr);

	struct firefly_transport_connection *conn_udp =
		firefly_transport_connection_udp_posix_new(llp, ipaddr, port, 1000);
	int res = firefly_connection_open(&actions, NULL, eq, conn_udp, NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	conn = tmp_conn;
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	firefly_transport_udp_posix_read(llp);
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(data_received);
	data_received = false;
	good_conn_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
}

void test_recv_conn_and_data()
{
	struct sockaddr_in remote_addr;
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_conn_recv_conn, eq);
	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	// send data
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	mock_test_event_queue_reset(eq);
	event_execute_test(eq, 1);

	CU_ASSERT_EQUAL(nbr_added_events, 2);
	CU_ASSERT_EQUAL(test_event_ids[0], test_event_deps[1][0]);

	CU_ASSERT_TRUE(good_conn_received);
	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
}

void test_recv_conn_and_two_data()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_conn_recv_conn, eq);
	struct sockaddr_in remote_addr;

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	// send data
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(good_conn_received);
	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;

	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));
	firefly_transport_udp_posix_read(llp);
	event_execute_test(eq, 1);

	CU_ASSERT_FALSE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
}

void test_recv_conn_keep()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_conn_recv_conn, eq);

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

// send data
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(good_conn_received);
	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(data_received);
	struct firefly_connection *conn = find_connection(llp, &remote_addr,
									connection_eq_inaddr);
	CU_ASSERT_PTR_NOT_NULL(conn);

	good_conn_received = false;
	data_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
}

int64_t recv_conn_keep_two(struct firefly_transport_llp *llp,
		const char *ipaddr, unsigned short port)
{
	good_conn_received = true;
	struct firefly_transport_connection *conn_udp =
		firefly_transport_connection_udp_posix_new(llp, ipaddr, port, 0);

	int64_t res = firefly_connection_open(NULL, NULL, eq, conn_udp, NULL);
	return res;
}

void test_recv_conn_keep_two()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
						local_port, recv_conn_keep_two, eq);

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	// send data
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	event_execute_test(eq, 1);

	// test first connection
	CU_ASSERT_TRUE(good_conn_received);
	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(data_received);
	struct firefly_connection *conn = find_connection(llp, &remote_addr,
									connection_eq_inaddr);
	CU_ASSERT_NOT_EQUAL(conn, NULL);

	good_conn_received = false;
	data_received = false;

	send_data(&remote_addr, 55558, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	event_execute_test(eq, 1);

	// test first connection
	CU_ASSERT_TRUE(good_conn_received);
	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(data_received);
	conn = find_connection(llp, &remote_addr, connection_eq_inaddr);
	CU_ASSERT_NOT_EQUAL(conn, NULL);

	good_conn_received = false;
	data_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
}

int64_t recv_conn_reject_recv_conn(struct firefly_transport_llp *llp,
		const char *ip_addr, unsigned short port)
{
	UNUSED_VAR(llp);
	UNUSED_VAR(ip_addr);
	UNUSED_VAR(port);
	good_conn_received = true;
	return 0;
}

void test_recv_conn_reject()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_conn_reject_recv_conn, eq);
	// send data
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_FALSE(data_received);

	CU_ASSERT_EQUAL(llp->conn_list, NULL);
	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
}

void test_conn_open_and_send()
{
	struct firefly_connection *conn;
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
							local_port, NULL, eq);

	struct firefly_connection_actions actions = {
		.connection_opened = tmp_on_conn_open,
	};
	struct firefly_transport_connection *conn_udp =
		firefly_transport_connection_udp_posix_new(llp,
				"127.0.0.1", 55550, 1000);
	int res = firefly_connection_open(&actions, NULL, eq, conn_udp, NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	conn = tmp_conn;

	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	struct sockaddr_in recv_addr;
	setup_sockaddr(&recv_addr, 55550);
	int recv_soc = open_socket(&recv_addr);

	firefly_transport_udp_posix_write(send_buf, sizeof(send_buf),
			conn, false, NULL);

	recv_data(recv_soc);

	close(recv_soc);
	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
}

void test_conn_open_and_recv()
{
	struct firefly_connection *conn;
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_data_recv_conn, eq);
	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);
	struct firefly_connection_actions actions = {
		.connection_opened = tmp_on_conn_open,
	};
	struct firefly_transport_connection *conn_udp =
		firefly_transport_connection_udp_posix_new(llp,
				"127.0.0.1", 55550, 1000);
	int res = firefly_connection_open(&actions, NULL, eq, conn_udp, NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	conn = tmp_conn;
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	struct sockaddr_in send_addr;
	send_data(&send_addr, 55550, send_buf, sizeof(send_buf));

	firefly_transport_udp_posix_read(llp);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(data_received);

	data_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
}

int64_t open_and_recv_conn_recv_conn(struct firefly_transport_llp *llp,
		const char *ipaddr, unsigned short port)
{
	good_conn_received = true;
	struct firefly_transport_connection *conn_udp =
		firefly_transport_connection_udp_posix_new(llp, ipaddr, port, 1000);

	int res = firefly_connection_open(NULL, NULL, eq, conn_udp, NULL);
	return res;
}

void test_open_and_recv_with_two_llp()
{
	struct firefly_transport_llp *llp_recv = 
		firefly_transport_llp_udp_posix_new(local_port,
				open_and_recv_conn_recv_conn, eq);
	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp_recv, protocol_data_received_repl);

	struct firefly_transport_llp *llp_send =
		firefly_transport_llp_udp_posix_new(remote_port,
					recv_data_recv_conn, eq);
	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp_send, protocol_data_received_repl);

	struct firefly_connection_actions actions = {
		.connection_opened = tmp_on_conn_open,
	};

	struct firefly_transport_connection *conn_udp =
		firefly_transport_connection_udp_posix_new(llp_send,
				"127.0.0.1", local_port, 1000);
	int res = firefly_connection_open(&actions, NULL, eq, conn_udp, NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	struct firefly_connection *conn_send = tmp_conn;
	CU_ASSERT_PTR_NOT_NULL(conn_send);

	firefly_transport_udp_posix_write(send_buf, sizeof(send_buf),
						conn_send, false, NULL);

	firefly_transport_udp_posix_read(llp_recv);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(good_conn_received);
	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;
	CU_ASSERT_PTR_NOT_NULL_FATAL(llp_recv->conn_list);
	struct firefly_connection *conn_recv = llp_recv->conn_list->conn;
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn_recv);

	firefly_transport_udp_posix_write(send_buf, sizeof(send_buf),
						conn_recv, false, NULL);

	firefly_transport_udp_posix_read(llp_send);
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(data_received);

	data_received = false;
	firefly_transport_llp_udp_posix_free(llp_recv);
	firefly_transport_llp_udp_posix_free(llp_send);
	event_execute_all_test(eq);
}

void test_recv_big_connection()
{
	struct firefly_transport_llp *llp =
		firefly_transport_llp_udp_posix_new(local_port,
							recv_conn_recv_conn, eq);

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	data_received = false;		/* test globals */
	good_conn_received = false;	/* ditto ... */
	data_recv_size = sizeof(send_buf);
	data_recv_buf = send_buf;

	firefly_transport_udp_posix_read(llp);
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(good_conn_received);
	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(data_received);

	send_data(&remote_addr, remote_port, send_buf_big,
			sizeof(send_buf_big));

	data_received = false;		/* same test globals ... */
	good_conn_received = false;
	data_recv_size = sizeof(send_buf_big);
	data_recv_buf = send_buf_big;

	firefly_transport_udp_posix_read(llp);
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(data_received);

	data_received = false;
	/* good_conn_received = false; */
	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
	data_recv_size = sizeof(send_buf);
	data_recv_buf = send_buf;
}

void *reader_thread_main(void *args)
{
	struct firefly_transport_llp *llp = (struct firefly_transport_llp *) args;
	firefly_transport_udp_posix_read(llp);
	struct transport_llp_udp_posix *llp_udp =
		(struct transport_llp_udp_posix *) llp->llp_platspec;
	event_execute_test(llp_udp->event_queue, 1);

	return NULL;
}

void test_read_mult_threads()
{
	struct sockaddr_in remote_addr;
	struct firefly_connection_actions actions = {
		.connection_opened = tmp_on_conn_open,
	};
	char ipaddr[INET_ADDRSTRLEN];
	unsigned short port;
	struct firefly_connection *conn;
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_data_recv_conn, eq);
	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	setup_sockaddr(&remote_addr, remote_port);
	sockaddr_in_ipaddr(&remote_addr, ipaddr);
	port = sockaddr_in_port(&remote_addr);

	struct firefly_transport_connection *conn_udp =
		firefly_transport_connection_udp_posix_new(llp, ipaddr, port, 1000);
	int res = firefly_connection_open(&actions, NULL, eq, conn_udp, NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	conn = tmp_conn;
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	pthread_t reader_thread;
	pthread_create(&reader_thread, NULL, reader_thread_main, llp);
	sched_yield();
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	pthread_join(reader_thread, NULL);

	CU_ASSERT_TRUE(data_received);
	data_received = false;
	good_conn_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
}

void test_llp_free_empty()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_data_recv_conn, eq);

	firefly_transport_llp_udp_posix_free(llp);
	event_execute_test(eq, 1);
}

void test_llp_free_mult_conns()
{
	int res;
	struct firefly_connection_actions actions = {
		.connection_opened = tmp_on_conn_open,
	};
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_data_recv_conn, eq);
	res = firefly_connection_open(&actions, NULL, eq,
			firefly_transport_connection_udp_posix_new(llp,
				"127.0.0.1", remote_port, 1000), NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	res = firefly_connection_open(&actions, NULL, eq,
			firefly_transport_connection_udp_posix_new(llp,
				"127.0.0.1", remote_port, 1000), NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	res = firefly_connection_open(&actions, NULL, eq,
			firefly_transport_connection_udp_posix_new(llp,
				"127.0.0.1", remote_port, 1000), NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	firefly_transport_llp_udp_posix_free(llp);
	event_execute_test(eq, 7);
}

// test correct number of events and channel close packets sent in the
// correct order.
void test_llp_free_mult_conns_w_chans()
{
	int res;
	struct firefly_connection *conn;
	struct firefly_channel *ch;
	struct firefly_connection_actions actions = {
		.connection_opened = tmp_on_conn_open,
	};
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, NULL, eq);

	res = firefly_connection_open(&actions, NULL, eq,
			firefly_transport_connection_udp_posix_new(llp,
				"127.0.0.1", 55550, 1000), NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	conn = tmp_conn;
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	ch = firefly_channel_new(conn);
	ch->remote_id = 1;
	add_channel_to_connection(ch, conn);

	res = firefly_connection_open(&actions, NULL, eq,
			firefly_transport_connection_udp_posix_new(llp,
				"127.0.0.1", 55550, 1000), NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	conn = tmp_conn;
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	ch = firefly_channel_new(conn);
	ch->remote_id = 2;
	add_channel_to_connection(ch, conn);

	res = firefly_connection_open(&actions, NULL, eq,
			firefly_transport_connection_udp_posix_new(llp,
				"127.0.0.1", 55550, 1000), NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	conn = tmp_conn;
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	ch = firefly_channel_new(conn);
	ch->remote_id = 3;
	add_channel_to_connection(ch, conn);

	firefly_transport_llp_udp_posix_free(llp);
	// llp free
	event_execute_test(eq, 1);

	// connection close
	event_execute_test(eq, 1);
	// channel close
	event_execute_test(eq, 1);
	// channel free
	event_execute_test(eq, 1);

	// connection close
	event_execute_test(eq, 1);
	// channel close
	event_execute_test(eq, 1);
	// channel free
	event_execute_test(eq, 1);

	// connection close
	event_execute_test(eq, 1);
	// channel close
	event_execute_test(eq, 1);
	// channel free
	event_execute_test(eq, 1);
	// channel close
	event_execute_test(eq, 1);
	// channel free
	event_execute_test(eq, 1);

	// 3 conn free (llp free included in conn free)
	event_execute_test(eq, 3);
}

int time_ms_diff(struct timespec *from, struct timespec *to)
{
	int diff;

	diff = (to->tv_sec - from->tv_sec) * 1000;
	diff += (to->tv_nsec - from->tv_nsec) / 1000000;

	return diff;
}

void print_timepsec(struct timespec t)
{
	//printf("%ld:%ld", t.tv_sec, t.tv_nsec);
	printf("%f", t.tv_sec + t.tv_nsec / 1000000000.0);
}

void test_send_important()
{
	int res;
	struct firefly_connection *conn;
	struct firefly_connection_actions actions = {
		.connection_opened = tmp_on_conn_open,
	};
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
							local_port, NULL, eq);
	struct transport_llp_udp_posix *llp_udp =
		(struct transport_llp_udp_posix *) llp->llp_platspec;
	res = firefly_connection_open(&actions, NULL, eq,
			firefly_transport_connection_udp_posix_new(llp,
				"127.0.0.1", 55550,
				FIREFLY_TRANSPORT_UDP_POSIX_DEFAULT_TIMEOUT), NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	conn = tmp_conn;
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	unsigned char id;
	struct timespec before;
	clock_gettime(CLOCK_REALTIME, &before);
	firefly_transport_udp_posix_write(send_buf, sizeof(send_buf), conn, true, &id);
	struct timespec after;
	clock_gettime(CLOCK_REALTIME, &after);

	CU_ASSERT_PTR_NOT_NULL_FATAL(llp_udp->resend_queue->first);
	CU_ASSERT_PTR_NOT_NULL_FATAL(llp_udp->resend_queue->last);

	CU_ASSERT_TRUE(memcmp(send_buf, llp_udp->resend_queue->first->data,
				sizeof(send_buf)) == 0);

	bool test_before = time_ms_diff(&before,
			&llp_udp->resend_queue->first->resend_at) >=
			FIREFLY_TRANSPORT_UDP_POSIX_DEFAULT_TIMEOUT;
	bool test_after = time_ms_diff(&after,
			&llp_udp->resend_queue->first->resend_at) <=
			FIREFLY_TRANSPORT_UDP_POSIX_DEFAULT_TIMEOUT;
	CU_ASSERT_TRUE(test_before);
	CU_ASSERT_TRUE(test_after);
	if (!test_before) {
		printf("\n");
		printf("before:\t");
		print_timepsec(before);
		printf("\n");
		printf("at:\t");
		print_timepsec(llp_udp->resend_queue->first->resend_at);
		printf("\n");
		printf("diff:\t%d\n", time_ms_diff(&before,
					&llp_udp->resend_queue->first->resend_at));
	}
	if (!test_after) {
		printf("\n");
		printf("after:\t");
		print_timepsec(after);
		printf("\n");
		printf("at:\t");
		print_timepsec(llp_udp->resend_queue->first->resend_at);
		printf("\n");
		printf("diff:\t%d\n", time_ms_diff(&after,
					&llp_udp->resend_queue->first->resend_at));
	}

	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
}

void test_send_important_ack()
{
	int res;
	struct firefly_connection *conn;
	struct firefly_connection_actions actions = {
		.connection_opened = tmp_on_conn_open,
	};
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
							local_port, NULL, eq);
	struct transport_llp_udp_posix *llp_udp =
		(struct transport_llp_udp_posix *) llp->llp_platspec;
	res = firefly_connection_open(&actions, NULL, eq,
			firefly_transport_connection_udp_posix_new(llp,
				"127.0.0.1", 55550,
				FIREFLY_TRANSPORT_UDP_POSIX_DEFAULT_TIMEOUT), NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	conn = tmp_conn;
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	unsigned char id;
	firefly_transport_udp_posix_write(send_buf, sizeof(send_buf), conn, true, &id);

	CU_ASSERT_PTR_NOT_NULL_FATAL(llp_udp->resend_queue->first);
	CU_ASSERT_PTR_NOT_NULL_FATAL(llp_udp->resend_queue->last);

	firefly_transport_udp_posix_ack(id, conn);

	CU_ASSERT_PTR_NULL(llp_udp->resend_queue->first);
	CU_ASSERT_PTR_NULL(llp_udp->resend_queue->last);

	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
}

void test_send_important_id_null()
{
	int res;
	struct firefly_connection *conn;
	struct firefly_connection_actions actions = {
		.connection_opened = tmp_on_conn_open,
	};
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
							local_port, NULL, eq);
	struct transport_llp_udp_posix *llp_udp =
		(struct transport_llp_udp_posix *) llp->llp_platspec;
	res = firefly_connection_open(&actions, NULL, eq,
			firefly_transport_connection_udp_posix_new(llp,
				"127.0.0.1", 55550,
				FIREFLY_TRANSPORT_UDP_POSIX_DEFAULT_TIMEOUT), NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	conn = tmp_conn;
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	expected_error = FIREFLY_ERROR_TRANS_WRITE;
	firefly_transport_udp_posix_write(send_buf, sizeof(send_buf), conn, true, NULL);
	CU_ASSERT_TRUE(was_in_error);

	CU_ASSERT_PTR_NULL(llp_udp->resend_queue->first);
	CU_ASSERT_PTR_NULL(llp_udp->resend_queue->last);

	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
	expected_error = FIREFLY_ERROR_FIRST;
}

void test_send_important_long_timeout()
{
	int res;
	struct firefly_connection *conn;
	struct firefly_connection_actions actions = {
		.connection_opened = tmp_on_conn_open,
	};
	int long_timeout = 1999;
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
							local_port, NULL, eq);
	struct transport_llp_udp_posix *llp_udp =
		(struct transport_llp_udp_posix *) llp->llp_platspec;
	res = firefly_connection_open(&actions, NULL, eq,
			firefly_transport_connection_udp_posix_new(llp,
				"127.0.0.1", 55550, long_timeout), NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	conn = tmp_conn;
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	unsigned char id;
	struct timespec before;
	clock_gettime(CLOCK_REALTIME, &before);
	firefly_transport_udp_posix_write(send_buf, sizeof(send_buf), conn, true, &id);
	struct timespec after;
	clock_gettime(CLOCK_REALTIME, &after);

	CU_ASSERT_PTR_NOT_NULL_FATAL(llp_udp->resend_queue->first);
	CU_ASSERT_PTR_NOT_NULL_FATAL(llp_udp->resend_queue->last);

	CU_ASSERT_TRUE(memcmp(send_buf, llp_udp->resend_queue->first->data,
				sizeof(send_buf)) == 0);

	bool test_before = time_ms_diff(&before,
			&llp_udp->resend_queue->first->resend_at) >= long_timeout;
	bool test_after = time_ms_diff(&after,
			&llp_udp->resend_queue->first->resend_at) <= long_timeout;
	CU_ASSERT_TRUE(test_before);
	CU_ASSERT_TRUE(test_after);
	if (!test_before) {
		printf("\n");
		printf("before:\t");
		print_timepsec(before);
		printf("\n");
		printf("at:\t");
		print_timepsec(llp_udp->resend_queue->first->resend_at);
		printf("\n");
	}
	if (!test_after) {
		printf("\n");
		printf("after:\t");
		print_timepsec(after);
		printf("\n");
		printf("at:\t");
		print_timepsec(llp_udp->resend_queue->first->resend_at);
		printf("\n");
	}

	firefly_transport_llp_udp_posix_free(llp);
	event_execute_all_test(eq);
}
