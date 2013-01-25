/**
 * @file
 * @brief The public API of the transport Ethernet POSIX with specific structures and
 * functions.
 */
#ifdef _GNU_SOURCE
#error "Something turned it on!"
#undef _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE (200112L)

#include "transport/firefly_transport_eth_posix_private.h"
#include <transport/firefly_transport_eth_posix.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/select.h>		// defines fd_set
#include <sys/ioctl.h>		// defines SIOCGIFINDEX
#include <unistd.h>			// defines close
#include <netinet/ether.h>	// defines ETH_P_ALL, AF_PACKET
#include <netpacket/packet.h>	// defines sockaddr_ll
#include <arpa/inet.h>		// defines htons
#include <linux/if.h>		// defines ifreq, IFNAMSIZ

#include <utils/firefly_event_queue.h>
#include <utils/firefly_errors.h>
#include <transport/firefly_transport.h>

#include "utils/firefly_event_queue_private.h"
#include "transport/firefly_transport_private.h"
#include "protocol/firefly_protocol_private.h"

#define ERROR_STR_MAX_LEN      (256)

struct firefly_transport_llp *firefly_transport_llp_eth_posix_new(
		const char *iface_name,
		firefly_on_conn_recv_eth_posix on_conn_recv,
		struct firefly_event_queue *event_queue)
{
	int err;
	struct ifreq ifr;
	struct sockaddr_ll addr;
	struct transport_llp_eth_posix *llp_eth;
	struct firefly_transport_llp *llp;
	llp_eth = malloc(sizeof(*llp_eth));
	
	/* Create the socket */
	err = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL));
	if(err < 0){
		firefly_error(FIREFLY_ERROR_SOCKET, 2,
					  "Failed in %s().\n", __FUNCTION__);
	}
	llp_eth->socket = err;

	/* Set the interface name */
	strncpy(ifr.ifr_name, iface_name, IFNAMSIZ);

	/* Retreive the interface index of the interface and save it to
	* ifr.ifr_ifindex. */
	err = ioctl(llp_eth->socket, SIOCGIFINDEX, &ifr);
	if(err < 0){
		close(llp_eth->socket);
		firefly_error(FIREFLY_ERROR_SOCKET, 2,
					  "Failed in %s().\n", __FUNCTION__);
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
	err = bind(llp_eth->socket, (struct sockaddr *)&addr,
		    sizeof(struct sockaddr_ll));
	if(err < 0){
		close(llp_eth->socket);
		firefly_error(FIREFLY_ERROR_SOCKET, 2,
					  "Failed in %s().\n", __FUNCTION__);
	}

	llp_eth->event_queue = event_queue;
	llp_eth->on_conn_recv = on_conn_recv;
	llp = malloc(sizeof(struct firefly_transport_llp));
	llp->llp_platspec = llp_eth;
	llp->conn_list = NULL;
	llp->protocol_data_received_cb = protocol_data_received;

	return llp;
}

void firefly_transport_llp_eth_posix_free(struct firefly_transport_llp *llp)
{
	struct firefly_event *ev = firefly_event_new(FIREFLY_PRIORITY_LOW,
			firefly_transport_llp_eth_posix_free_event, llp);
	struct transport_llp_eth_posix *llp_eth;
	llp_eth = (struct transport_llp_eth_posix *) llp->llp_platspec;
	int ret = llp_eth->event_queue->offer_event_cb(llp_eth->event_queue, ev);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "could not add event to queue");
	}
}

int firefly_transport_llp_eth_posix_free_event(void *event_arg)
{
	struct firefly_transport_llp *llp =
		(struct firefly_transport_llp *) event_arg;
	struct transport_llp_eth_posix *llp_eth;
	llp_eth = (struct transport_llp_eth_posix *) llp->llp_platspec;

	bool empty = true;
	// Close all connections.
	struct llp_connection_list_node *head = llp->conn_list;
	while (head != NULL) {
		empty = false;
		firefly_connection_close(head->conn);
		head = head->next;
	}
	if (empty) {
		close(llp_eth->socket);
		free(llp_eth);
		free(llp);
	} else {
		firefly_transport_llp_eth_posix_free(llp);
	}
	return 0;
}

void firefly_transport_connection_eth_posix_free(struct firefly_connection *conn)
{
	struct protocol_connection_eth_posix *conn_eth;
	conn_eth =
		(struct protocol_connection_eth_posix *)
		conn->transport_conn_platspec;

	remove_connection_from_llp(conn_eth->llp, conn,
			firefly_connection_eq_ptr);
	free(conn_eth->remote_addr);
	free(conn_eth);
}

struct firefly_connection *firefly_transport_connection_eth_posix_open(
				firefly_channel_is_open_f on_channel_opened,
				firefly_channel_closed_f on_channel_closed,
				firefly_channel_accept_f on_channel_recv,
				char *mac_address,
				char *if_name,
				struct firefly_transport_llp *llp)
{
	/* Get ifindex first to avoid mem alloc if it fails. */
	int err;
	struct ifreq ifr;
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	err = ioctl(((struct transport_llp_eth_posix *)llp->llp_platspec)->socket,
			SIOCGIFINDEX, &ifr);
	if(err < 0){
			firefly_error(FIREFLY_ERROR_SOCKET, 3,
					"Failed in %s() on line %d.\n", __FUNCTION__,
					__LINE__);
			return NULL;
	}

	/* Alloc connection structs */
	struct protocol_connection_eth_posix *conn_eth;
	conn_eth = malloc(sizeof(struct protocol_connection_eth_posix));
	struct firefly_connection *conn = firefly_connection_new_register(
			on_channel_opened, on_channel_closed, on_channel_recv,
			firefly_transport_eth_posix_write,
			((struct transport_llp_eth_posix *)llp->llp_platspec)->event_queue,
			conn_eth, firefly_transport_connection_eth_posix_free, true);
	if (conn == NULL || conn_eth == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
				"Failed in %s() on line %d.\n", __FUNCTION__,
				__LINE__);
		free(conn_eth);
		free(conn);
		return NULL;
	}
	conn_eth->remote_addr = malloc(sizeof(struct sockaddr_ll));
	if (conn_eth == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
				"Failed in %s() on line %d.\n", __FUNCTION__,
				__LINE__);
		free(conn_eth);
		return NULL;
	}

	memset(conn_eth->remote_addr, 0, sizeof(struct sockaddr_ll));
	conn_eth->remote_addr->sll_family   = AF_PACKET;
	conn_eth->remote_addr->sll_protocol = htons(FIREFLY_ETH_PROTOCOL);
	/* Convert address to to binary data */
	ether_aton_r(mac_address, (void*)&conn_eth->remote_addr->sll_addr);
	conn_eth->remote_addr->sll_halen    = 6;
	conn_eth->remote_addr->sll_ifindex  = ifr.ifr_ifindex;

	conn_eth->socket =
		((struct transport_llp_eth_posix *)llp->llp_platspec)->socket;

	conn_eth->llp = llp;

	add_connection_to_llp(conn, llp);

	return conn;
}

void firefly_transport_eth_posix_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn)
{
	int err;
	struct protocol_connection_eth_posix *conn_eth =
		(struct protocol_connection_eth_posix *) conn->transport_conn_platspec;
	err = sendto(conn_eth->socket, data, data_size, 0,
			(struct sockaddr *)conn_eth->remote_addr,
			sizeof(*conn_eth->remote_addr));
	if (err == -1) {
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s() on line %d.\nFailed to select.", __FUNCTION__,
				__LINE__);
	}
}

void firefly_transport_eth_posix_read(struct firefly_transport_llp *llp,
		struct timeval *tv)
{
	int res;
	struct transport_llp_eth_posix *llp_eth =
		(struct transport_llp_eth_posix *) llp->llp_platspec;
	fd_set fs;
	FD_ZERO(&fs);
	FD_SET(llp_eth->socket, &fs);
	res = select(llp_eth->socket + 1, &fs, NULL, NULL, tv);
	if (res == 0) {
		return;
	}
	if (res == -1) {
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s() on line %d.\nFailed to select.", __FUNCTION__,
				__LINE__);
		return;
	}

	struct firefly_event_llp_read_eth_posix *ev_arg =
		malloc(sizeof(struct firefly_event_llp_read_eth_posix));
	if (ev_arg == NULL) {
			firefly_error(FIREFLY_ERROR_ALLOC, 3,
					"Failed in %s() on line %d.\n",
					__FUNCTION__, __LINE__);
	}
	ev_arg->addr = malloc(sizeof(struct sockaddr_ll));
	if (ev_arg->addr == NULL) {
			firefly_error(FIREFLY_ERROR_ALLOC, 3,
					"Failed in %s() on line %d.\n",
					__FUNCTION__, __LINE__);
	}
	ev_arg->len = 0;
	ev_arg->data = NULL;
	ev_arg->llp = llp;

	// Read data from socket, = 0 is crucial due to ioctl only sets the
	// first 32 bits of pkg_len
	res = ioctl(llp_eth->socket, FIONREAD, &ev_arg->len);
	if (res == -1) {
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s() on line %d.\nCould not get next packet size.", __FUNCTION__,
				__LINE__);
		ev_arg->len = 0;
	}

	ev_arg->data = malloc(ev_arg->len);
	if (ev_arg->data == NULL) {
			firefly_error(FIREFLY_ERROR_ALLOC, 3,
					"Failed in %s() on line %d.\n",
					__FUNCTION__, __LINE__);
	}

	socklen_t addr_len = sizeof(struct sockaddr_ll);
	res = recvfrom(llp_eth->socket, ev_arg->data, ev_arg->len, 0,
			(struct sockaddr *) ev_arg->addr, &addr_len);
	if (res == -1) {
		char err_buf[ERROR_STR_MAX_LEN];
		strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s.\n%s()\nCould not read from socket.", __FUNCTION__, err_buf);
	}

	struct firefly_event *ev = firefly_event_new(FIREFLY_PRIORITY_HIGH,
			firefly_transport_eth_posix_read_event, ev_arg);
	llp_eth->event_queue->offer_event_cb(llp_eth->event_queue, ev);
}

int firefly_transport_eth_posix_read_event(void *event_args)
{
	struct firefly_event_llp_read_eth_posix *ev_a =
		(struct firefly_event_llp_read_eth_posix *) event_args;
	struct transport_llp_eth_posix *llp_eth =
		(struct transport_llp_eth_posix *) ev_a->llp->llp_platspec;

	struct firefly_connection *conn = find_connection(ev_a->llp, ev_a->addr,
			connection_eq_addr);
	if (conn == NULL) {
		if (llp_eth->on_conn_recv != NULL) {
			char mac_addr[18];
			get_mac_addr(ev_a->addr, mac_addr);
			// TODO got incorrect mac address here once, ever... two times.
			conn = llp_eth->on_conn_recv(ev_a->llp, mac_addr);
		} else {
			firefly_error(FIREFLY_ERROR_MISSING_CALLBACK, 4,
				      "Cannot accept incoming connection.\n"
				      "Callback 'on_conn_recv' not set"
				      "on llp.\n (in %s() at %s:%d)",
				      __FUNCTION__, __FILE__, __LINE__);
		}
	}
	// Existing or newly created conn. Passing data to procol layer.
	if (conn != NULL) {
		ev_a->llp->protocol_data_received_cb(conn, ev_a->data, ev_a->len);
	}
	free(ev_a->data);
	free(ev_a->addr);
	free(ev_a);
	return 0;
}

void get_mac_addr(struct sockaddr_ll *addr, char *mac_addr)
{
	char temp_addr[18];
	unsigned int addr_vals[6];
	ether_ntoa_r((void*)addr->sll_addr, temp_addr);
	sscanf(temp_addr, "%x:%x:%x:%x:%x:%x", addr_vals+0, addr_vals+1,
			addr_vals+2, addr_vals+3, addr_vals+4, addr_vals+5);
	sprintf(mac_addr, "%02x:%02x:%02x:%02x:%02x:%02x", addr_vals[0],
			addr_vals[1], addr_vals[2], addr_vals[3], addr_vals[4],
			addr_vals[5]);
}

bool connection_eq_addr(struct firefly_connection *conn, void *context)
{
	struct protocol_connection_eth_posix *conn_eth =
		(struct protocol_connection_eth_posix *) conn->transport_conn_platspec;
	struct sockaddr_ll *addr = (struct sockaddr_ll *) context;
	int result;
	if (addr->sll_halen == conn_eth->remote_addr->sll_halen) {
		result = memcmp(conn_eth->remote_addr->sll_addr, addr->sll_addr,
				addr->sll_halen);
	} else {
		result = 1;
	}
	return result == 0;
}

