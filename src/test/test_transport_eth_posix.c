/**
 * @file
 * @brief Test the transport layer with POSIX Ethernet.
 */


#include "test/test_transport_eth_posix.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <netinet/ether.h>	// defines ETH_P_ALL
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>		// defines htons
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if.h>		// defines ifreq
#include <linux/net.h>		// defines SOCK_DGRAM, AF_PACKET

#include "CUnit/Basic.h"
#include "CUnit/Console.h"

#include <transport/firefly_transport.h>
#include <transport/firefly_transport_eth_posix.h>
#include <protocol/firefly_protocol.h>

#include "transport/firefly_transport_private.h"
#include "transport/firefly_transport_eth_posix_private.h"
#include "protocol/firefly_protocol_private.h"
#include "test/event_helper.h"
#include "utils/cppmacros.h"
#include "test_transport.h"

#define ETH_DST_ADDR_START	(0)
#define ETH_SRC_ADDR_START	(6)
#define ETH_PROTOCOL_START	(12)
#define ETH_DATA_START	(14)
#define ETH_HEADER_LEN	(14)

static struct firefly_event_queue *eq = NULL;

extern unsigned int nbr_added_events;
extern int64_t test_event_ids[50];
extern int64_t test_event_deps[50][FIREFLY_EVENT_QUEUE_MAX_DEPENDS];

int init_suit_eth_posix()
{
	eq = firefly_event_queue_new(mock_test_event_add, 10, NULL);
	return 0; // Success.
}

int clean_suit_eth_posix()
{
	firefly_event_queue_free(&eq);
	return 0; // Success.
}

extern bool data_received;
extern size_t data_recv_size;
extern unsigned char *data_recv_buf;
extern unsigned char send_buf[16];
extern struct firefly_connection *data_recv_expected_conn;

static char *remote_mac_addr = "00:00:00:00:00:01";
static char *remote_mac_addr_alt = "00:00:00:00:00:02";
static char *local_mac_addr = "00:00:00:00:00:00";
static char *if_name = "lo";

static int open_socket()
{
	int err;
	int socket_fd;
	struct ifreq ifr;
	struct sockaddr_ll addr;
	
	/* Create the socket */
	err = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if(err < 0){
		CU_FAIL("Could not open socket.");
		return err;
	}
	socket_fd = err;

	/* Set the interface name */
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	/* Retreive the interface index of the interface and save it to
	* ifr.ifr_ifindex.
	*/
	err = ioctl(socket_fd, SIOCGIFINDEX, &ifr);
	if(err < 0){
		CU_FAIL("Could not get interface index.");
		close(socket_fd);
		return err;
	}

	/* init mem of local_addr with zeros */
	memset(&addr, 0, sizeof(struct sockaddr_ll));
	/* set address parameters */
	addr.sll_family   = AF_PACKET;
	/* htons will convert protocol into correct format used by network */
	addr.sll_protocol = htons(FIREFLY_ETH_PROTOCOL);
	/*addr.sll_protocol = htons(ETH_P_ALL);*/
	addr.sll_ifindex  = ifr.ifr_ifindex;
	/* Bind socket to specified interface */
	err = bind(socket_fd, (struct sockaddr *)&addr,
		    sizeof(struct sockaddr_ll));
	if(err < 0){
		CU_FAIL("Could not bind socket.");
		close(socket_fd);
		return err;
	}
	
	return socket_fd;
}

static void send_data_w_addr(char *mac_addr)
{
	unsigned char buffer[ETH_HEADER_LEN+sizeof(send_buf)];
	unsigned short protocol;
	struct sockaddr_ll addr;
	struct ifreq ifr;
	int err;
	int socket = open_socket();
	if (socket < 0) {
		return;
	}
	ether_aton_r(local_mac_addr, (void*)buffer + ETH_DST_ADDR_START);
	ether_aton_r(mac_addr, (void*)buffer + ETH_SRC_ADDR_START);
	protocol = htons(FIREFLY_ETH_PROTOCOL);
	memcpy(buffer+ETH_PROTOCOL_START, &protocol, 2);
	memcpy(buffer+ETH_DATA_START, send_buf, sizeof(send_buf));

	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	err = ioctl(socket, SIOCGIFINDEX, &ifr);
	if(err < 0){
		CU_FAIL("Could not get interface index.");
		close(socket);
	}
	memset(&addr, 0, sizeof(struct sockaddr_ll));
	addr.sll_ifindex  = ifr.ifr_ifindex;

	err = sendto(socket, buffer, sizeof(buffer), 0,
			(struct sockaddr *)&addr, sizeof(addr));
	if (err == -1) {
		CU_FAIL("Failed to send data.");
	}
	close(socket);
}

static void send_data()
{
	send_data_w_addr(remote_mac_addr);
}

static void recv_data(int socket)
{
	unsigned char recv_buf[sizeof(send_buf)];
	struct sockaddr_ll remote_addr;
	socklen_t len = sizeof(struct sockaddr_ll);
	int res = recvfrom(socket, recv_buf, sizeof(send_buf), 0,
			(struct sockaddr *) &remote_addr, &len);
	if (res == -1) {
		CU_FAIL("Failed to receive data from socket.\n");
	}
	CU_ASSERT_EQUAL(sizeof(send_buf), res);
	CU_ASSERT_NSTRING_EQUAL(recv_buf, send_buf, sizeof(send_buf));
}

static bool recv_conn_called = false;
static int64_t on_conn_recv(
		struct firefly_transport_llp *llp, char *mac_address)
{
	UNUSED_VAR(llp);
	if (memcmp(mac_address, remote_mac_addr, 18) != 0) {
		printf("\nAct. mac: %s\n", mac_address);
		printf("Exp. mac: %s\n", remote_mac_addr);
	}
	CU_ASSERT_NSTRING_EQUAL(mac_address, remote_mac_addr, 18);
	recv_conn_called = true;
	return 0;
}

static int64_t on_conn_recv_keep(
		struct firefly_transport_llp *llp, char *mac_address)
{
	UNUSED_VAR(llp);
	CU_ASSERT_NSTRING_EQUAL(mac_address, remote_mac_addr, 18);
	if (memcmp(mac_address, remote_mac_addr, 18) != 0) {
		printf("Mac: %s\n", mac_address);
	}
	recv_conn_called = true;
	int64_t res = firefly_connection_open(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				mac_address, "lo"), NULL);
	CU_ASSERT_TRUE(res > 0);
	return res;
}

static int64_t on_conn_recv_keep_two(
		struct firefly_transport_llp *llp, char *mac_address)
{
	UNUSED_VAR(llp);
	recv_conn_called = true;
	int64_t res = firefly_connection_open(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				mac_address, "lo"), NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	return res;
}

void test_eth_recv_connection()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv, eq);

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	send_data();

	firefly_transport_eth_posix_read(llp, NULL);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(recv_conn_called);
	firefly_transport_llp_eth_posix_free(llp);
	event_execute_all_test(eq);
	recv_conn_called = false;
	data_received = false;
}

void test_eth_recv_conn_null_cb()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", NULL, eq);

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	send_data();

	mock_test_event_queue_reset(eq);
	firefly_transport_eth_posix_read(llp, NULL);
	CU_ASSERT_EQUAL(nbr_added_events, 1);
	mock_test_event_queue_reset(eq);
	event_execute_test(eq, 1);

	CU_ASSERT_EQUAL(nbr_added_events, 0);
	CU_ASSERT_FALSE(data_received);
	firefly_transport_llp_eth_posix_free(llp);
	event_execute_all_test(eq);
	data_received = false;
}

void test_eth_recv_data()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv, eq);

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	send_data();
	int res = firefly_connection_open(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				remote_mac_addr, "lo"), NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	firefly_transport_eth_posix_read(llp, NULL);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(data_received);
	data_received = false;
	CU_ASSERT_FALSE(recv_conn_called);
	recv_conn_called = false;
	firefly_transport_llp_eth_posix_free(llp);
	event_execute_all_test(eq);
}

void test_eth_recv_conn_and_data()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep, eq);

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	send_data();

	firefly_transport_eth_posix_read(llp, NULL);
	mock_test_event_queue_reset(eq);
	event_execute_test(eq, 1);

	CU_ASSERT_EQUAL(nbr_added_events, 2);
	CU_ASSERT_EQUAL(test_event_ids[0], test_event_deps[1][0]);

	CU_ASSERT_TRUE(recv_conn_called);
	recv_conn_called = false;
	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(data_received);
	data_received = false;
	firefly_transport_llp_eth_posix_free(llp);
	event_execute_all_test(eq);
}

void test_eth_recv_conn_keep()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep, eq);

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	send_data();

	firefly_transport_eth_posix_read(llp, NULL);
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(recv_conn_called);
	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(data_received);

	struct sockaddr_ll addr;
	ether_aton_r(remote_mac_addr, (void*)&addr.sll_addr);
	addr.sll_halen    = 6;
	struct firefly_connection *conn = find_connection(llp, &addr, connection_eq_addr);
	CU_ASSERT_PTR_NOT_NULL(conn);

	recv_conn_called = false;
	data_received = false;
	firefly_transport_llp_eth_posix_free(llp);
	event_execute_all_test(eq);
}

void test_eth_recv_conn_reject()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv, eq);
	send_data();

	firefly_transport_eth_posix_read(llp, NULL);
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(recv_conn_called);
	CU_ASSERT_FALSE(data_received);

	struct sockaddr_ll addr;
	ether_aton_r(local_mac_addr, (void*)&addr.sll_addr);
	addr.sll_halen    = 6;
	struct firefly_connection *conn = find_connection(llp, &addr, connection_eq_addr);
	CU_ASSERT_PTR_NULL(conn);

	recv_conn_called = false;
	data_received = false;
	firefly_transport_llp_eth_posix_free(llp);
	event_execute_all_test(eq);
}

void test_eth_recv_conn_and_two_data()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep, eq);

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	send_data();

	firefly_transport_eth_posix_read(llp, NULL);
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(recv_conn_called);
	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(data_received);

	struct sockaddr_ll addr;
	ether_aton_r(remote_mac_addr, (void*)&addr.sll_addr);
	addr.sll_halen    = 6;
	struct firefly_connection *conn = find_connection(llp, &addr, connection_eq_addr);
	CU_ASSERT_PTR_NOT_NULL(conn);
	recv_conn_called = false;
	data_received = false;

	send_data();

	firefly_transport_eth_posix_read(llp, NULL);
	event_execute_test(eq, 1);
	CU_ASSERT_FALSE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	recv_conn_called = false;
	data_received = false;
	firefly_transport_llp_eth_posix_free(llp);
	event_execute_all_test(eq);
}

void test_eth_conn_open_and_send()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv, eq);

	struct firefly_connection *conn = firefly_connection_new(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				remote_mac_addr, "lo"));
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);
	add_connection_to_llp(conn, llp);

	int socket = open_socket();

	firefly_transport_eth_posix_write(send_buf, sizeof(send_buf),
			conn, false, NULL);

	recv_data(socket);
	close(socket);
	firefly_transport_llp_eth_posix_free(llp);
	event_execute_all_test(eq);
}

void test_eth_conn_open_and_recv()
{
	test_eth_recv_data();
}

void test_eth_recv_conn_keep_two()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep_two, eq);

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	send_data_w_addr(remote_mac_addr);
	firefly_transport_eth_posix_read(llp, NULL);
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(recv_conn_called);
	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(data_received);

	struct sockaddr_ll addr;
	ether_aton_r(remote_mac_addr, (void*)&addr.sll_addr);
	addr.sll_halen = 6;
	struct firefly_connection *conn_1 = find_connection(llp, &addr, connection_eq_addr);
	CU_ASSERT_PTR_NOT_NULL(conn_1);

	recv_conn_called = false;
	data_received = false;

	send_data_w_addr(remote_mac_addr_alt);
	firefly_transport_eth_posix_read(llp, NULL);
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(recv_conn_called);
	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(data_received);

	ether_aton_r(remote_mac_addr_alt, (void*)&addr.sll_addr);
	addr.sll_halen = 6;
	struct firefly_connection *conn_2 = find_connection(llp, &addr, connection_eq_addr);
	CU_ASSERT_PTR_NOT_NULL(conn_2);

	CU_ASSERT_PTR_NOT_EQUAL(conn_1, conn_2);

	recv_conn_called = false;
	data_received = false;

	firefly_transport_llp_eth_posix_free(llp);
	event_execute_all_test(eq);
}

void test_eth_recv_data_two_conn()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep_two, eq);

	/* Replace the ordinary data recv. callback. */
	replace_protocol_data_received_cb(llp, protocol_data_received_repl);

	struct firefly_connection *conn_1 = firefly_connection_new(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				remote_mac_addr, "lo"));
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn_1);
	add_connection_to_llp(conn_1, llp);

	struct firefly_connection *conn_2 = firefly_connection_new(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				remote_mac_addr_alt, "lo"));
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn_2);
	add_connection_to_llp(conn_2, llp);

	data_recv_expected_conn = conn_1;
	send_data_w_addr(remote_mac_addr);
	firefly_transport_eth_posix_read(llp, NULL);
	event_execute_test(eq, 1);
	CU_ASSERT_FALSE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	data_recv_expected_conn = conn_2;
	send_data_w_addr(remote_mac_addr_alt);
	firefly_transport_eth_posix_read(llp, NULL);
	event_execute_test(eq, 1);
	CU_ASSERT_FALSE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	recv_conn_called = false;
	data_received = false;
	data_recv_expected_conn = NULL;
	firefly_transport_llp_eth_posix_free(llp);
	event_execute_all_test(eq);
}

void test_eth_conn_close_recv()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv, eq);

	struct firefly_connection *conn = firefly_connection_new(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				remote_mac_addr, "lo"));
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);
	add_connection_to_llp(conn, llp);

	firefly_connection_close(conn);
	send_data();
	firefly_transport_eth_posix_read(llp, NULL);
	event_execute_test(eq, 1);

	CU_ASSERT_FALSE(data_received);
	CU_ASSERT_FALSE(recv_conn_called);

	firefly_transport_llp_eth_posix_free(llp);
	event_execute_all_test(eq);
}

void test_eth_llp_free_empty()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
					"lo", NULL, eq);

	firefly_transport_llp_eth_posix_free(llp);
	event_execute_test(eq, 1);
}

void test_eth_llp_free_mult_conns()
{
	struct firefly_connection *conn;
	struct firefly_transport_llp *llp;

	llp = firefly_transport_llp_eth_posix_new("lo", NULL, eq);

	conn = firefly_connection_new(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				"00:00:00:00:00:00", "lo"));
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);
	add_connection_to_llp(conn, llp);

	conn = firefly_connection_new(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				"00:00:00:00:00:00", "lo"));
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);
	add_connection_to_llp(conn, llp);

	conn = firefly_connection_new(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				"00:00:00:00:00:00", "lo"));
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);
	add_connection_to_llp(conn, llp);

	firefly_transport_llp_eth_posix_free(llp);
	event_execute_test(eq, 7);
}

void test_eth_llp_free_mult_conns_w_chans()
{
	// test correct number of events and channel close packets sent in the
	// correct order.
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
					"lo", NULL, eq);
	struct firefly_connection *conn;
	struct firefly_channel *ch;

	conn = firefly_connection_new(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				"00:00:00:00:00:00", "lo"));
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);
	add_connection_to_llp(conn, llp);

	ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	ch = firefly_channel_new(conn);
	ch->remote_id = 1;
	add_channel_to_connection(ch, conn);

	conn = firefly_connection_new(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				"00:00:00:00:00:00", "lo"));
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);
	add_connection_to_llp(conn, llp);

	ch = firefly_channel_new(conn);
	ch->remote_id = 2;
	add_channel_to_connection(ch, conn);

	conn = firefly_connection_new(NULL, NULL, eq,
			firefly_transport_connection_eth_posix_new(llp,
				"00:00:00:00:00:00", "lo"));
	CU_ASSERT_PTR_NOT_NULL_FATAL(conn);
	add_connection_to_llp(conn, llp);

	ch = firefly_channel_new(conn);
	ch->remote_id = 3;
	add_channel_to_connection(ch, conn);

	firefly_transport_llp_eth_posix_free(llp);
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

	// 3 conn free (includes llp free)
	event_execute_test(eq, 3);
}
