/**
 * @file
 * @brief Test the transport layer with POSIX UDP.
 */
#include <pthread.h>
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
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_udp_posix.h>

#include "transport/firefly_transport_private.h"
#include "transport/firefly_transport_udp_posix_private.h"
#include "protocol/firefly_protocol_private.h"
#include "utils/cppmacros.h"

extern unsigned char send_buf[16];
extern bool data_received;
extern size_t data_recv_size;
extern unsigned char *data_recv_buf;

int init_suit_udp_posix()
{
	return 0; // Success.
}

int clean_suit_udp_posix()
{
	return 0; // Success.
}

static const unsigned short local_port = 55555;
static unsigned short remote_port = 55556;

static unsigned char send_buf_big[] = {0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
				   0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
				   0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
				   0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};


static void execute_remaining_events(struct firefly_event_queue *eq)
{
	struct firefly_event *ev;
	while (firefly_event_queue_length(eq) > 0) {
		ev = firefly_event_pop(eq);
		CU_ASSERT_PTR_NOT_NULL(ev);
		firefly_event_execute(ev);
	}
}

static void execute_events(struct firefly_event_queue *eq, size_t nbr_events)
{
	struct firefly_event *ev;
	for (size_t i = 0; i < nbr_events; i++) {
		ev = firefly_event_pop(eq);
		CU_ASSERT_PTR_NOT_NULL(ev);
		firefly_event_execute(ev);
	}
}

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
struct firefly_connection *recv_conn_recv_conn(
		struct firefly_transport_llp *llp, const char *ip_addr, unsigned short port)
{
	CU_ASSERT_STRING_EQUAL(ip_addr, "127.0.0.1");
	CU_ASSERT_EQUAL(port, remote_port);
	good_conn_received = true;
	struct firefly_connection *conn =
			firefly_transport_connection_udp_posix_open(NULL, NULL, NULL,
					ip_addr, port, llp);
	return conn;
}

/* Test receiving a new connection.*/
void test_recv_connection()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
						local_port, recv_conn_recv_conn, eq);
	struct sockaddr_in remote_addr;

	// send data
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);
	CU_ASSERT_TRUE(good_conn_received);
	data_received = false;
	good_conn_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

struct firefly_connection *recv_data_recv_conn(
		struct firefly_transport_llp *llp, const char *ip_addr, unsigned short port)
{
	UNUSED_VAR(llp);
	UNUSED_VAR(ip_addr);
	UNUSED_VAR(port);
	CU_FAIL("Received connection but shouldn't have.\n");
	return NULL;
}

void test_recv_data()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_data_recv_conn, eq);
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	struct protocol_connection_udp_posix *conn_udp =
		malloc(sizeof(struct protocol_connection_udp_posix));

	conn_udp->remote_addr = malloc(sizeof(struct sockaddr_in));
	memcpy(conn_udp->remote_addr, &remote_addr, sizeof(remote_addr));
	conn_udp->llp = llp;
	struct firefly_connection *conn = firefly_connection_new(NULL, NULL,
			NULL, NULL, eq, conn_udp, firefly_transport_connection_udp_posix_free);

	add_connection_to_llp(conn, llp);

	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);
	CU_ASSERT_TRUE(data_received);
	data_received = false;
	good_conn_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_recv_conn_and_data()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_conn_recv_conn, eq);
	struct sockaddr_in remote_addr;

	// send data
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_recv_conn_and_two_data()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_conn_recv_conn, eq);
	struct sockaddr_in remote_addr;

	// send data
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;

	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));
	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);

	CU_ASSERT_FALSE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_recv_conn_keep()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_conn_recv_conn, eq);
	// send data
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	struct firefly_connection *conn = find_connection(llp, &remote_addr,
									connection_eq_inaddr);
	CU_ASSERT_PTR_NOT_NULL(conn);

	good_conn_received = false;
	data_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

struct firefly_connection *recv_conn_keep_two(
		struct firefly_transport_llp *llp, const char *ip_addr, unsigned short port)
{
	good_conn_received = true;
	struct firefly_connection *conn =
			firefly_transport_connection_udp_posix_open(NULL, NULL, NULL,
					ip_addr, port, llp);
	return conn;
}

void test_recv_conn_keep_two()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
						local_port, recv_conn_keep_two, eq);
	// send data
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);

	// test first connection
	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	struct firefly_connection *conn = find_connection(llp, &remote_addr,
									connection_eq_inaddr);
	CU_ASSERT_NOT_EQUAL(conn, NULL);

	good_conn_received = false;
	data_received = false;

	send_data(&remote_addr, 55558, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);

	// test first connection
	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	conn = find_connection(llp, &remote_addr, connection_eq_inaddr);
	CU_ASSERT_NOT_EQUAL(conn, NULL);

	good_conn_received = false;
	data_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

struct firefly_connection *recv_conn_reject_recv_conn(
		struct firefly_transport_llp *llp, const char *ip_addr, unsigned short port)
{
	UNUSED_VAR(llp);
	UNUSED_VAR(ip_addr);
	UNUSED_VAR(port);
	good_conn_received = true;
	return NULL;
}

void test_recv_conn_reject()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_conn_reject_recv_conn, eq);
	// send data
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	// Set up a connection over local loopback.
	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);

	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_FALSE(data_received);

	CU_ASSERT_EQUAL(llp->conn_list, NULL);
	firefly_transport_llp_udp_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

// NOTE: This test is supposed to segfault if it fails as that is the only way
// to test it.
void test_null_pointer_as_callback()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
							local_port, NULL, eq);
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);

	CU_PASS("Passed null pointer as callback\n");

	firefly_transport_llp_udp_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_conn_open_and_send()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
							local_port, NULL, eq);
	struct firefly_connection *conn = 
		firefly_transport_connection_udp_posix_open(NULL, NULL, NULL,
				"127.0.0.1", 55550, llp);
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	struct sockaddr_in recv_addr;
	setup_sockaddr(&recv_addr, 55550);
	int recv_soc = open_socket(&recv_addr);

	firefly_transport_udp_posix_write(send_buf, sizeof(send_buf), conn);

	recv_data(recv_soc);

	close(recv_soc);
	firefly_transport_llp_udp_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_conn_open_and_recv()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_data_recv_conn, eq);

	struct firefly_connection *conn =
		firefly_transport_connection_udp_posix_open(NULL, NULL, NULL,
				"127.0.0.1", 55550, llp);
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);

	struct sockaddr_in send_addr;
	send_data(&send_addr, 55550, send_buf, sizeof(send_buf));

	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);

	CU_ASSERT_TRUE(data_received);

	data_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

static struct firefly_connection *conn_recv = NULL;
/* Callback when a new connection arrives at transport layer. */
struct firefly_connection *open_and_recv_conn_recv_conn(
		struct firefly_transport_llp *llp, const char *ip_addr, unsigned short port)
{
	good_conn_received = true;
	conn_recv = firefly_transport_connection_udp_posix_open(NULL, NULL, NULL,
			ip_addr, port, llp);
	return conn_recv;
}

void test_open_and_recv_with_two_llp()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp_recv = 
		firefly_transport_llp_udp_posix_new(local_port,
				open_and_recv_conn_recv_conn, eq);

	struct firefly_transport_llp *llp_send =
		firefly_transport_llp_udp_posix_new(remote_port,
					recv_data_recv_conn, eq);

	struct firefly_connection *conn_send = 
		firefly_transport_connection_udp_posix_open(NULL, NULL, NULL,
				"127.0.0.1", local_port, llp_send);
	CU_ASSERT_PTR_NOT_NULL(conn_send);

	firefly_transport_udp_posix_write(send_buf, sizeof(send_buf),
						conn_send);

	firefly_transport_udp_posix_read(llp_recv);
	execute_events(eq, 1);

	CU_ASSERT_PTR_NOT_NULL(conn_recv);
	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);
	good_conn_received = false;
	data_received = false;

	firefly_transport_udp_posix_write(send_buf, sizeof(send_buf),
						conn_recv);

	firefly_transport_udp_posix_read(llp_send);
	execute_events(eq, 1);
	CU_ASSERT_TRUE(data_received);

	data_received = false;
	firefly_transport_llp_udp_posix_free(llp_recv);
	firefly_transport_llp_udp_posix_free(llp_send);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_recv_big_connection()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp =
		firefly_transport_llp_udp_posix_new(local_port,
							recv_conn_recv_conn, eq);
	struct sockaddr_in remote_addr;
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	data_received = false;		/* test globals */
	good_conn_received = false;	/* ditto ... */
	data_recv_size = sizeof(send_buf);
	data_recv_buf = send_buf;

	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);
	CU_ASSERT_TRUE(good_conn_received);
	CU_ASSERT_TRUE(data_received);

	send_data(&remote_addr, remote_port, send_buf_big,
			sizeof(send_buf_big));

	data_received = false;		/* same test globals ... */
	good_conn_received = false;
	data_recv_size = sizeof(send_buf_big);
	data_recv_buf = send_buf_big;

	firefly_transport_udp_posix_read(llp);
	execute_events(eq, 1);
	CU_ASSERT_TRUE(data_received);

	data_received = false;
	/* good_conn_received = false; */
	firefly_transport_llp_udp_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
	data_recv_size = sizeof(send_buf);
	data_recv_buf = send_buf;
}

void *reader_thread_main(void *args)
{
	struct firefly_transport_llp *llp = (struct firefly_transport_llp *) args;
	firefly_transport_udp_posix_read(llp);
	struct transport_llp_udp_posix *llp_udp =
		(struct transport_llp_udp_posix *) llp->llp_platspec;
	execute_events(llp_udp->event_queue, 1);

	return NULL;
}

void test_read_mult_threads()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_data_recv_conn, eq);

	struct sockaddr_in remote_addr;
	setup_sockaddr(&remote_addr, remote_port);
	struct protocol_connection_udp_posix *conn_udp =
		malloc(sizeof(struct protocol_connection_udp_posix));
	conn_udp->remote_addr = malloc(sizeof(struct sockaddr_in));
	memcpy(conn_udp->remote_addr, &remote_addr, sizeof(remote_addr));
	conn_udp->llp = llp;
	struct firefly_connection *conn = firefly_connection_new(NULL, NULL,
			NULL, NULL, eq, conn_udp,
			firefly_transport_connection_udp_posix_free);
	add_connection_to_llp(conn, llp);

	pthread_t reader_thread;
	pthread_create(&reader_thread, NULL, reader_thread_main, llp);
	sched_yield();
	send_data(&remote_addr, remote_port, send_buf, sizeof(send_buf));

	pthread_join(reader_thread, NULL);

	CU_ASSERT_TRUE(data_received);
	data_received = false;
	good_conn_received = false;
	firefly_transport_llp_udp_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_llp_free_empty()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_data_recv_conn, eq);

	firefly_transport_llp_udp_posix_free(llp);
	execute_events(eq, 1);
	firefly_event_queue_free(&eq);
}

void test_llp_free_mult_conns()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, recv_data_recv_conn, eq);
	struct firefly_connection *conn;

	conn = firefly_connection_udp_posix_new(NULL, NULL, NULL, llp, NULL);
	add_connection_to_llp(conn, llp);

	conn = firefly_connection_udp_posix_new(NULL, NULL, NULL, llp, NULL);
	add_connection_to_llp(conn, llp);

	conn = firefly_connection_udp_posix_new(NULL, NULL, NULL, llp, NULL);
	add_connection_to_llp(conn, llp);

	firefly_transport_llp_udp_posix_free(llp);
	execute_events(eq, 8);
	firefly_event_queue_free(&eq);
}

static bool data_sent = false;
static void recv_socket_chan_close(int socket)
{
	int res;
	size_t pkg_len = 0;
	res = ioctl(socket, FIONREAD, &pkg_len);
	if (res == -1) {
		printf("Could not read recv data size\n");
		return;
	}
	if (pkg_len <= 0) {
		printf("Expected to receive data but did not.\n");
		return;
	}
	unsigned char *recv_buf = malloc(pkg_len);
	struct sockaddr_in remote_addr;
	socklen_t len = sizeof(struct sockaddr_in);
	res = recvfrom(socket, recv_buf, pkg_len, 0,
			(struct sockaddr *) &remote_addr, &len);
	free(recv_buf);
	if (res == -1) {
		CU_FAIL("Failed to receive data from socket.\n");
		return;
	}
	data_sent = true;
}

void test_llp_free_mult_conns_w_chans()
{
	// test correct number of events and channel close packets sent in the
	// correct order.
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_udp_posix_new(
					local_port, NULL, eq);
	struct firefly_connection *conn;
	struct firefly_channel *ch;

	conn = firefly_transport_connection_udp_posix_open(NULL, NULL, NULL,
			"127.0.0.1", 55550, llp);

	ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	ch = firefly_channel_new(conn);
	ch->remote_id = 1;
	add_channel_to_connection(ch, conn);

	conn = firefly_transport_connection_udp_posix_open(NULL, NULL, NULL,
			"127.0.0.1", 55550, llp);

	ch = firefly_channel_new(conn);
	ch->remote_id = 2;
	add_channel_to_connection(ch, conn);

	conn = firefly_transport_connection_udp_posix_open(NULL, NULL, NULL,
			"127.0.0.1", 55550, llp);

	ch = firefly_channel_new(conn);
	ch->remote_id = 3;
	add_channel_to_connection(ch, conn);

	struct sockaddr_in addr;
	setup_sockaddr(&addr, 55550);
	int socket = open_socket(&addr);
	firefly_transport_llp_udp_posix_free(llp);
	// llp free
	execute_events(eq, 1);

	// connection close
	execute_events(eq, 1);
	// channel close
	execute_events(eq, 1);
	recv_socket_chan_close(socket);
	CU_ASSERT_TRUE(data_sent);
	data_sent = false;
	// channel free
	execute_events(eq, 1);

	// connection close
	execute_events(eq, 1);
	// channel close
	execute_events(eq, 1);
	recv_socket_chan_close(socket);
	CU_ASSERT_TRUE(data_sent);
	data_sent = false;
	// channel free
	execute_events(eq, 1);

	// connection close
	execute_events(eq, 1);
	// channel close
	execute_events(eq, 1);
	recv_socket_chan_close(socket);
	CU_ASSERT_TRUE(data_sent);
	data_sent = false;
	// channel free
	execute_events(eq, 1);
	// channel close
	execute_events(eq, 1);
	recv_socket_chan_close(socket);
	CU_ASSERT_TRUE(data_sent);
	data_sent = false;
	// channel free
	execute_events(eq, 1);

	// 3 conn close, 3 conn free, llp free
	execute_events(eq, 7);
	close(socket);
	firefly_event_queue_free(&eq);
	data_sent = false;
}
