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
#include "utils/cppmacros.h"

#define ETH_DST_ADDR_START	(0)
#define ETH_SRC_ADDR_START	(6)
#define ETH_PROTOCOL_START	(12)
#define ETH_DATA_START	(14)
#define ETH_HEADER_LEN	(14)


int init_suit_eth_posix()
{
	return 0; // Success.
}

int clean_suit_eth_posix()
{
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
static struct firefly_connection *on_conn_recv(
		struct firefly_transport_llp *llp, char *mac_address)
{
	UNUSED_VAR(llp);
	CU_ASSERT_NSTRING_EQUAL(mac_address, remote_mac_addr, 18);
	if (memcmp(mac_address, remote_mac_addr, 18) != 0) {
		printf("Mac: %s\n", mac_address);
	}
	recv_conn_called = true;
	return NULL;
}

static struct firefly_connection *on_conn_recv_keep(
		struct firefly_transport_llp *llp, char *mac_address)
{
	UNUSED_VAR(llp);
	CU_ASSERT_NSTRING_EQUAL(mac_address, remote_mac_addr, 18);
	if (memcmp(mac_address, remote_mac_addr, 18) != 0) {
		printf("Mac: %s\n", mac_address);
	}
	recv_conn_called = true;
	return firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, mac_address, "lo", llp);
}

static struct firefly_connection *on_conn_recv_keep_two(
		struct firefly_transport_llp *llp, char *mac_address)
{
	UNUSED_VAR(llp);
	recv_conn_called = true;
	return firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, mac_address, "lo", llp);
}

void test_eth_recv_connection()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv, eq);
	send_data();

	firefly_transport_eth_posix_read(llp);
	execute_events(eq, 1);

	CU_ASSERT_TRUE(recv_conn_called);
	recv_conn_called = false;
	firefly_transport_llp_eth_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_eth_recv_data()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv, eq);
	send_data();
	firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, remote_mac_addr, "lo", llp);

	firefly_transport_eth_posix_read(llp);
	execute_events(eq, 1);

	CU_ASSERT_TRUE(data_received);
	data_received = false;
	CU_ASSERT_FALSE(recv_conn_called);
	recv_conn_called = false;
	firefly_transport_llp_eth_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_eth_recv_conn_and_data()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep, eq);
	send_data();

	firefly_transport_eth_posix_read(llp);
	execute_events(eq, 1);

	CU_ASSERT_TRUE(recv_conn_called);
	recv_conn_called = false;
	CU_ASSERT_TRUE(data_received);
	data_received = false;
	firefly_transport_llp_eth_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_eth_recv_conn_keep()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep, eq);
	send_data();

	firefly_transport_eth_posix_read(llp);
	execute_events(eq, 1);
	CU_ASSERT_TRUE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	struct sockaddr_ll addr;
	ether_aton_r(remote_mac_addr, (void*)&addr.sll_addr);
	addr.sll_halen    = 6;
	struct firefly_connection *conn = find_connection(llp, &addr, connection_eq_addr);
	CU_ASSERT_PTR_NOT_NULL(conn);

	recv_conn_called = false;
	data_received = false;
	firefly_transport_llp_eth_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_eth_recv_conn_reject()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv, eq);
	send_data();

	firefly_transport_eth_posix_read(llp);
	execute_events(eq, 1);
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
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_eth_recv_conn_and_two_data()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep, eq);
	send_data();

	firefly_transport_eth_posix_read(llp);
	execute_events(eq, 1);
	CU_ASSERT_TRUE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	struct sockaddr_ll addr;
	ether_aton_r(remote_mac_addr, (void*)&addr.sll_addr);
	addr.sll_halen    = 6;
	struct firefly_connection *conn = find_connection(llp, &addr, connection_eq_addr);
	CU_ASSERT_PTR_NOT_NULL(conn);
	recv_conn_called = false;
	data_received = false;

	send_data();

	firefly_transport_eth_posix_read(llp);
	execute_events(eq, 1);
	CU_ASSERT_FALSE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	recv_conn_called = false;
	data_received = false;
	firefly_transport_llp_eth_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_eth_conn_open_and_send()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv, eq);
	struct firefly_connection *conn = firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, remote_mac_addr, "lo", llp);

	int socket = open_socket();

	firefly_transport_eth_posix_write(send_buf, sizeof(send_buf), conn);

	recv_data(socket);
	close(socket);
	firefly_transport_llp_eth_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_eth_conn_open_and_recv()
{
	test_eth_recv_data();
}

void test_eth_recv_conn_keep_two()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep_two, eq);

	send_data_w_addr(remote_mac_addr);
	firefly_transport_eth_posix_read(llp);
	execute_events(eq, 1);
	CU_ASSERT_TRUE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	struct sockaddr_ll addr;
	ether_aton_r(remote_mac_addr, (void*)&addr.sll_addr);
	addr.sll_halen = 6;
	struct firefly_connection *conn_1 = find_connection(llp, &addr, connection_eq_addr);
	CU_ASSERT_PTR_NOT_NULL(conn_1);

	recv_conn_called = false;
	data_received = false;

	send_data_w_addr(remote_mac_addr_alt);
	firefly_transport_eth_posix_read(llp);
	execute_events(eq, 1);
	CU_ASSERT_TRUE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	ether_aton_r(remote_mac_addr_alt, (void*)&addr.sll_addr);
	addr.sll_halen = 6;
	struct firefly_connection *conn_2 = find_connection(llp, &addr, connection_eq_addr);
	CU_ASSERT_PTR_NOT_NULL(conn_2);

	CU_ASSERT_PTR_NOT_EQUAL(conn_1, conn_2);

	recv_conn_called = false;
	data_received = false;

	firefly_transport_llp_eth_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_eth_recv_data_two_conn()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep_two, eq);
	struct firefly_connection *conn_1 = firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, remote_mac_addr, "lo", llp);
	struct firefly_connection *conn_2 = firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, remote_mac_addr_alt, "lo", llp);

	data_recv_expected_conn = conn_1;
	send_data_w_addr(remote_mac_addr);
	firefly_transport_eth_posix_read(llp);
	execute_events(eq, 1);
	CU_ASSERT_FALSE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	data_recv_expected_conn = conn_2;
	send_data_w_addr(remote_mac_addr_alt);
	firefly_transport_eth_posix_read(llp);
	execute_events(eq, 1);
	CU_ASSERT_FALSE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	recv_conn_called = false;
	data_received = false;
	data_recv_expected_conn = NULL;
	firefly_transport_llp_eth_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_eth_conn_close_recv()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv, eq);
	struct firefly_connection *conn = firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, remote_mac_addr, "lo", llp);
	firefly_connection_close(conn);
	send_data();
	firefly_transport_eth_posix_read(llp);
	execute_events(eq, 1);

	CU_ASSERT_FALSE(data_received);
	CU_ASSERT_FALSE(recv_conn_called);

	firefly_transport_llp_eth_posix_free(llp);
	execute_remaining_events(eq);
	firefly_event_queue_free(&eq);
}

void test_eth_llp_free_empty()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
					"lo", NULL, eq);

	firefly_transport_llp_eth_posix_free(llp);
	execute_events(eq, 1);
	firefly_event_queue_free(&eq);
}

void test_eth_llp_free_mult_conns()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
					"lo", NULL, eq);
	struct firefly_connection *conn;

	conn = firefly_transport_connection_eth_posix_open(NULL, NULL, NULL,
			"00:00:00:00:00:00", "lo", llp);

	conn = firefly_transport_connection_eth_posix_open(NULL, NULL, NULL,
			"00:00:00:00:00:00", "lo", llp);

	conn = firefly_transport_connection_eth_posix_open(NULL, NULL, NULL,
			"00:00:00:00:00:00", "lo", llp);

	firefly_transport_llp_eth_posix_free(llp);
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
	struct sockaddr_ll remote_addr;
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

void test_eth_llp_free_mult_conns_w_chans()
{
	// test correct number of events and channel close packets sent in the
	// correct order.
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
					"lo", NULL, eq);
	struct firefly_connection *conn;
	struct firefly_channel *ch;

	conn = firefly_transport_connection_eth_posix_open(NULL, NULL, NULL,
			"00:00:00:00:00:00", "lo", llp);

	ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	ch = firefly_channel_new(conn);
	ch->remote_id = 1;
	add_channel_to_connection(ch, conn);

	conn = firefly_transport_connection_eth_posix_open(NULL, NULL, NULL,
			"00:00:00:00:00:00", "lo", llp);

	ch = firefly_channel_new(conn);
	ch->remote_id = 2;
	add_channel_to_connection(ch, conn);

	conn = firefly_transport_connection_eth_posix_open(NULL, NULL, NULL,
			"00:00:00:00:00:00", "lo", llp);

	ch = firefly_channel_new(conn);
	ch->remote_id = 3;
	add_channel_to_connection(ch, conn);

	int socket = open_socket();
	firefly_transport_llp_eth_posix_free(llp);
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
