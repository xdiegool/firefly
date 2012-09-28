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
			NULL, NULL, NULL, NULL, mac_address, "lo", llp);
}

static struct firefly_connection *on_conn_recv_keep_two(
		struct firefly_transport_llp *llp, char *mac_address)
{
	UNUSED_VAR(llp);
	recv_conn_called = true;
	return firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, NULL, mac_address, "lo", llp);
}

void test_eth_recv_connection()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv);
	send_data();

	firefly_transport_eth_posix_read(llp);

	CU_ASSERT_TRUE(recv_conn_called);
	recv_conn_called = false;
	firefly_transport_llp_eth_posix_free(&llp);
}

void test_eth_recv_data()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv);
	send_data();
	struct firefly_connection *conn = firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, NULL, remote_mac_addr, "lo", llp);

	firefly_transport_eth_posix_read(llp);

	CU_ASSERT_TRUE(data_received);
	data_received = false;
	CU_ASSERT_FALSE(recv_conn_called);
	recv_conn_called = false;
	firefly_transport_llp_eth_posix_free(&llp);
}

void test_eth_recv_conn_and_data()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep);
	send_data();

	firefly_transport_eth_posix_read(llp);

	CU_ASSERT_TRUE(recv_conn_called);
	recv_conn_called = false;
	CU_ASSERT_TRUE(data_received);
	data_received = false;
	firefly_transport_llp_eth_posix_free(&llp);
}

void test_eth_recv_conn_keep()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep);
	send_data();

	firefly_transport_eth_posix_read(llp);
	CU_ASSERT_TRUE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	struct sockaddr_ll addr;
	ether_aton_r(remote_mac_addr, (void*)&addr.sll_addr);
	addr.sll_halen    = 6;
	struct firefly_connection *conn = find_connection(llp, &addr, connection_comp_addr);
	CU_ASSERT_PTR_NOT_NULL(conn);

	recv_conn_called = false;
	data_received = false;
	firefly_transport_llp_eth_posix_free(&llp);
}

void test_eth_recv_conn_reject()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv);
	send_data();

	firefly_transport_eth_posix_read(llp);
	CU_ASSERT_TRUE(recv_conn_called);
	CU_ASSERT_FALSE(data_received);

	struct sockaddr_ll addr;
	ether_aton_r(local_mac_addr, (void*)&addr.sll_addr);
	addr.sll_halen    = 6;
	struct firefly_connection *conn = find_connection(llp, &addr, connection_comp_addr);
	CU_ASSERT_PTR_NULL(conn);

	recv_conn_called = false;
	data_received = false;
	firefly_transport_llp_eth_posix_free(&llp);
}

void test_eth_recv_conn_and_two_data()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep);
	send_data();

	firefly_transport_eth_posix_read(llp);
	CU_ASSERT_TRUE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	struct sockaddr_ll addr;
	ether_aton_r(remote_mac_addr, (void*)&addr.sll_addr);
	addr.sll_halen    = 6;
	struct firefly_connection *conn = find_connection(llp, &addr, connection_comp_addr);
	CU_ASSERT_PTR_NOT_NULL(conn);
	recv_conn_called = false;
	data_received = false;

	send_data();

	firefly_transport_eth_posix_read(llp);
	CU_ASSERT_FALSE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	recv_conn_called = false;
	data_received = false;
	firefly_transport_llp_eth_posix_free(&llp);
}

void test_eth_conn_open_and_send()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv);
	struct firefly_connection *conn = firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, NULL, remote_mac_addr, "lo", llp);

	int socket = open_socket();

	firefly_transport_eth_posix_write(send_buf, sizeof(send_buf), conn);

	recv_data(socket);
	close(socket);
	firefly_transport_llp_eth_posix_free(&llp);
}

void test_eth_conn_open_and_recv()
{
	test_eth_recv_data();
}

void test_eth_recv_conn_keep_two()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep_two);

	send_data_w_addr(remote_mac_addr);
	firefly_transport_eth_posix_read(llp);
	CU_ASSERT_TRUE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	struct sockaddr_ll addr;
	ether_aton_r(remote_mac_addr, (void*)&addr.sll_addr);
	addr.sll_halen = 6;
	struct firefly_connection *conn_1 = find_connection(llp, &addr, connection_comp_addr);
	CU_ASSERT_PTR_NOT_NULL(conn_1);

	recv_conn_called = false;
	data_received = false;

	send_data_w_addr(remote_mac_addr_alt);
	firefly_transport_eth_posix_read(llp);
	CU_ASSERT_TRUE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	ether_aton_r(remote_mac_addr_alt, (void*)&addr.sll_addr);
	addr.sll_halen = 6;
	struct firefly_connection *conn_2 = find_connection(llp, &addr, connection_comp_addr);
	CU_ASSERT_PTR_NOT_NULL(conn_2);

	CU_ASSERT_PTR_NOT_EQUAL(conn_1, conn_2);

	recv_conn_called = false;
	data_received = false;

	firefly_transport_llp_eth_posix_free(&llp);
}

void test_eth_recv_data_two_conn()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv_keep_two);
	struct firefly_connection *conn_1 = firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, NULL, remote_mac_addr, "lo", llp);
	struct firefly_connection *conn_2 = firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, NULL, remote_mac_addr_alt, "lo", llp);

	data_recv_expected_conn = conn_1;
	send_data_w_addr(remote_mac_addr);
	firefly_transport_eth_posix_read(llp);
	CU_ASSERT_FALSE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	data_recv_expected_conn = conn_2;
	send_data_w_addr(remote_mac_addr_alt);
	firefly_transport_eth_posix_read(llp);
	CU_ASSERT_FALSE(recv_conn_called);
	CU_ASSERT_TRUE(data_received);

	recv_conn_called = false;
	data_received = false;
	data_recv_expected_conn = NULL;
	firefly_transport_llp_eth_posix_free(&llp);
}

void test_eth_conn_close_recv()
{
	struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(
			"lo", on_conn_recv);
	struct firefly_connection *conn = firefly_transport_connection_eth_posix_open(
			NULL, NULL, NULL, NULL, remote_mac_addr, "lo", llp);
	firefly_transport_connection_eth_posix_close(conn);
	send_data();
	firefly_transport_eth_posix_read(llp);

	CU_ASSERT_FALSE(data_received);
	CU_ASSERT_FALSE(recv_conn_called);

	firefly_transport_llp_eth_posix_free(&llp);
}

// Test if other data in the NIC might cause some wierd problem.
/*void test_eth_read()*/
/*{*/
	/*struct firefly_transport_llp *llp = firefly_transport_llp_eth_posix_new(*/
			/*"lo", on_conn_recv_keep);*/

	/*firefly_transport_eth_posix_read(llp);*/

	/*firefly_transport_llp_eth_posix_free(&llp);*/
/*}*/

int main()
{
	CU_pSuite trans_eth_posix = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	trans_eth_posix = CU_add_suite("eth_core", init_suit_eth_posix, clean_suit_eth_posix);
	if (trans_eth_posix == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	if (
		(CU_add_test(trans_eth_posix, "test_eth_recv_connection",
				test_eth_recv_connection) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_data",
				test_eth_recv_data) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_conn_and_data",
				test_eth_recv_conn_and_data) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_conn_keep",
				test_eth_recv_conn_keep) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_conn_reject",
				test_eth_recv_conn_reject) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_conn_and_two_data",
				test_eth_recv_conn_and_two_data) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_conn_open_and_send",
				test_eth_conn_open_and_send) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_conn_open_and_recv",
				test_eth_conn_open_and_recv) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_conn_keep_two",
				test_eth_recv_conn_keep_two) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_data_two_conn",
				test_eth_recv_data_two_conn) == NULL)
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