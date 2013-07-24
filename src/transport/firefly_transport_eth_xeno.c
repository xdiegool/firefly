/**
 * @file
 * @brief The public API of the transport Ethernet POSIX with specific structures and
 * functions.
 */

#include "transport/firefly_transport_eth_xeno_private.h"
#include <transport/firefly_transport_eth_xeno.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

// Xenomai includes
#include <native/task.h>
#include <rtnet.h>
#include <native/timer.h>
#include <rtdm/rtdm.h>

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
#include "utils/cppmacros.h"

#define ERROR_STR_MAX_LEN      (256)
#define XENO_HEAP_SIZE			(1024)

struct firefly_transport_llp *firefly_transport_llp_eth_xeno_new(
		const char *iface_name,
		firefly_on_conn_recv_eth_xeno on_conn_recv,
		struct firefly_event_queue *event_queue)
{
	int err;
	struct ifreq ifr;
	struct sockaddr_ll addr;
	struct transport_llp_eth_xeno *llp_eth;
	struct firefly_transport_llp *llp;
	llp_eth = malloc(sizeof(*llp_eth));

	/* Create the socket */
	err = rt_dev_socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL));
	if (err < 0) {
		FFL(FIREFLY_ERROR_SOCKET);
		free(llp_eth);
		return NULL;
	}

	llp_eth->socket = err;

	/* Set the interface name */
	strncpy(ifr.ifr_name, iface_name, IFNAMSIZ);

	/* Retreive the interface index of the interface and save it to
	* ifr.ifr_ifindex. */
	err = rt_dev_ioctl(llp_eth->socket, SIOCGIFINDEX, &ifr);
	if(err < 0){
		FFL(FIREFLY_ERROR_SOCKET);
		rt_dev_close(llp_eth->socket);
		free(llp_eth);
		return NULL;
	}

	/* init mem of local_addr with zeros */
	memset(&addr, 0, sizeof(struct sockaddr_ll));
	/* set address parameters */
	addr.sll_family   = AF_PACKET;
	/* htons will convert protocol into correct format used by network */
	addr.sll_protocol = htons(FIREFLY_ETH_PROTOCOL);
	/*addr.sll_protocol = htons(ETH_P_ALL);*/
	addr.sll_ifindex  = ifr.ifr_ifindex;
	/* Retreive the mac address of the interface and save it to ifr */
	err = rt_dev_ioctl(llp_eth->socket, SIOCGIFHWADDR, &ifr);
	if(err < 0){
		rt_dev_close(llp_eth->socket);
		free(llp_eth);
		FFL(FIREFLY_ERROR_SOCKET);
		return NULL;
	}
	/* 6 is the length of a mac address */
	memcpy(ifr.ifr_hwaddr.sa_data, addr.sll_addr, 6);
	addr.sll_halen = 6;
	/* Bind socket to specified interface */
	err = rt_dev_bind(llp_eth->socket, (struct sockaddr *)&addr,
		    sizeof(struct sockaddr_ll));
	if(err < 0){
		rt_dev_close(llp_eth->socket);
		free(llp_eth);
		FFL(FIREFLY_ERROR_SOCKET);
		return NULL;
	}
	err = rt_heap_create(&llp_eth->dyn_mem, "somename", XENO_HEAP_SIZE, H_MAPPABLE);
	if (err) {
		rt_dev_close(llp_eth->socket);
		free(llp_eth);
		FFL(FIREFLY_ERROR_ALLOC);
		return NULL;
	}

	llp_eth->event_queue		= event_queue;
	llp_eth->on_conn_recv		= on_conn_recv;

	llp				= malloc(sizeof(*llp));
	if (!llp) {
		rt_dev_close(llp_eth->socket);
		free(llp_eth);
		free(llp);
		FFL(FIREFLY_ERROR_ALLOC);
		return NULL;
	}
	llp->llp_platspec		= llp_eth;
	llp->conn_list			= NULL;
	llp->protocol_data_received_cb	= protocol_data_received;

	return llp;
}

void firefly_transport_llp_eth_xeno_free(struct firefly_transport_llp *llp)
{
	struct transport_llp_eth_xeno *llp_eth;
	llp_eth = (struct transport_llp_eth_xeno *) llp->llp_platspec;
	int ret = llp_eth->event_queue->offer_event_cb(llp_eth->event_queue,
			FIREFLY_PRIORITY_LOW, firefly_transport_llp_eth_xeno_free_event, llp);
	FFLIF(ret, FIREFLY_ERROR_ALLOC);
}

int firefly_transport_llp_eth_xeno_free_event(void *event_arg)
{
	struct firefly_transport_llp *llp;
	struct transport_llp_eth_xeno *llp_eth;

	llp = event_arg;
	llp_eth = llp->llp_platspec;

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
		rt_heap_delete(&llp_eth->dyn_mem);
		free(llp_eth);
		free(llp);
	} else {
		firefly_transport_llp_eth_xeno_free(llp);
	}
	return 0;
}

void firefly_transport_connection_eth_xeno_free(struct firefly_connection *conn)
{
	struct protocol_connection_eth_xeno *conn_eth;
	conn_eth = conn->transport_conn_platspec;

	remove_connection_from_llp(conn_eth->llp, conn,
			firefly_connection_eq_ptr);
	free(conn_eth->remote_addr);
	free(conn_eth);
}

static void *xeno_mem_alloc_llp(struct firefly_transport_llp *llp, size_t size)
{
	/*return malloc(size);*/
	void *buf;
	struct transport_llp_eth_xeno *llp_eth;
	llp_eth = llp->llp_platspec;
	int err = rt_heap_alloc(&llp_eth->dyn_mem, size, TM_NONBLOCK, &buf);
	if (err) {
		FFL(FIREFLY_ERROR_ALLOC);
		return NULL;
	} else {
		return buf;
	}
}

static void *xeno_mem_alloc_conn(struct firefly_connection *conn, size_t size)
{
	/*return malloc(size);*/
	struct protocol_connection_eth_xeno *conn_eth;
	conn_eth = conn->transport_conn_platspec;
	return xeno_mem_alloc_llp(conn_eth->llp, size);
}

static void xeno_mem_free_llp(struct firefly_transport_llp *llp, void *ptr)
{
	struct transport_llp_eth_xeno *llp_eth;
	llp_eth = llp->llp_platspec;
	int err = rt_heap_free(&llp_eth->dyn_mem, ptr);
	FFLIF(err, FIREFLY_ERROR_ALLOC);
}

static void xeno_mem_free_conn(struct firefly_connection *conn, void *ptr)
{
	/*free(ptr);*/
	struct protocol_connection_eth_xeno *conn_eth;
	conn_eth = conn->transport_conn_platspec;
	xeno_mem_free_llp(conn_eth->llp, ptr);
}

static struct firefly_memory_funcs conn_xeno_mem_fs = {
	.alloc_replacement = xeno_mem_alloc_conn,
	.free_replacement = xeno_mem_free_conn
};

struct firefly_connection *firefly_transport_connection_eth_xeno_open(
		struct firefly_transport_llp *llp,
		char *mac_address,
		char *if_name,
		struct firefly_connection_actions *actions)

{
	/* Get ifindex first to avoid mem alloc if it fails. */
	int err;
	struct ifreq ifr;
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	err = rt_dev_ioctl(((struct transport_llp_eth_xeno *)llp->llp_platspec)->socket,
			SIOCGIFINDEX, &ifr);
	if(err < 0) {
		FFL(FIREFLY_ERROR_SOCKET);
		return NULL;
	}

	/* Alloc connection structs */
	struct protocol_connection_eth_xeno *conn_eth;
	conn_eth = malloc(sizeof(*conn_eth));
	struct firefly_connection *conn = firefly_connection_new(
			actions,
			firefly_transport_eth_xeno_write,
			firefly_transport_eth_xeno_ack,
			&conn_xeno_mem_fs,
			((struct transport_llp_eth_xeno *)llp->llp_platspec)->event_queue,
			conn_eth, firefly_transport_connection_eth_xeno_free);
	if (conn == NULL || conn_eth == NULL) {
		FFL(FIREFLY_ERROR_ALLOC);
		free(conn_eth);
		free(conn);
		return NULL;
	}
	conn_eth->remote_addr = malloc(sizeof(struct sockaddr_ll));
	if (conn_eth == NULL) {
		FFL(FIREFLY_ERROR_ALLOC);
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
		((struct transport_llp_eth_xeno *)llp->llp_platspec)->socket;

	conn_eth->llp = llp;
	add_connection_to_llp(conn, llp);

	return conn;
}

void firefly_transport_eth_xeno_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id)
{
	int err;
	struct protocol_connection_eth_xeno *conn_eth =
		(struct protocol_connection_eth_xeno *) conn->transport_conn_platspec;
	err = rt_dev_sendto(conn_eth->socket, data, data_size, 0,
			(struct sockaddr *)conn_eth->remote_addr,
			sizeof(*conn_eth->remote_addr));
	if (err < 0) {
		FFL(FIREFLY_ERROR_SOCKET);
		if (conn->actions->connection_error)
			conn->actions->connection_error(conn);
	}
	if (important && id != NULL) {
		// TODO
	}
}

void firefly_transport_eth_xeno_ack(unsigned char pkt_id,
		struct firefly_connection *conn)
{
	UNUSED_VAR(pkt_id);
	UNUSED_VAR(conn);
}

void firefly_transport_eth_xeno_read(struct firefly_transport_llp *llp,
		int64_t *timeout)
{
	int res;
	struct transport_llp_eth_xeno *llp_eth;
	struct sockaddr_ll fromaddr;
	unsigned char tmp_buffer[1500];

	llp_eth = llp->llp_platspec;
	res = rt_dev_ioctl(llp_eth->socket, RTNET_RTIOC_TIMEOUT, timeout);
	if(res < 0){
		FFL(FIREFLY_ERROR_SOCKET);
		return;
	}

	socklen_t addr_len = sizeof(struct sockaddr_ll);
	res = rt_dev_recvfrom(llp_eth->socket, tmp_buffer, 1500, 0,
			(struct sockaddr *) &fromaddr, &addr_len);
	if (res == -EWOULDBLOCK || res == -EAGAIN || res == -EINTR ||
			res == -ETIMEDOUT) {
		return;
	} else if (res < 0) {
		char err_buf[ERROR_STR_MAX_LEN];
		strerror_r(-res, err_buf, ERROR_STR_MAX_LEN);
		firefly_error(FIREFLY_ERROR_SOCKET, 4,
			      "recvfrom() failed in %s().\n%s, %s\n",
			      __FUNCTION__, err_buf);
		return;
	}
	struct firefly_event_llp_read_eth_xeno *ev_arg =
		xeno_mem_alloc_llp(llp, sizeof(*ev_arg));
	if (!ev_arg) {
		FFL(FIREFLY_ERROR_ALLOC);
		return;
	}
	memcpy(&ev_arg->addr, &fromaddr, sizeof(struct sockaddr_ll));

	ev_arg->llp = llp;
	ev_arg->len = res;
	ev_arg->data = xeno_mem_alloc_llp(llp, ev_arg->len);
	if (!ev_arg->data) {
		FFL(FIREFLY_ERROR_ALLOC);
		return;
	}
	memcpy(ev_arg->data, tmp_buffer, ev_arg->len);

	llp_eth->event_queue->offer_event_cb(llp_eth->event_queue,
			FIREFLY_PRIORITY_HIGH,
			firefly_transport_eth_xeno_read_event, ev_arg);
}

int firefly_transport_eth_xeno_read_event(void *event_args)
{
	struct firefly_event_llp_read_eth_xeno *ev_a = event_args;
	struct transport_llp_eth_xeno *llp_eth = ev_a->llp->llp_platspec;

	struct firefly_connection *conn = find_connection(ev_a->llp, &ev_a->addr,
			connection_eq_addr);
	if (conn == NULL) {
		if (llp_eth->on_conn_recv != NULL) {
			char mac_addr[18];
			get_mac_addr(&ev_a->addr, mac_addr);
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
	xeno_mem_free_llp(ev_a->llp, ev_a->data);
	xeno_mem_free_llp(ev_a->llp, ev_a);
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
	struct protocol_connection_eth_xeno *conn_eth;
	struct sockaddr_ll *addr;
	int result;

	conn_eth = conn->transport_conn_platspec;
	addr = context;

	if (addr->sll_halen == conn_eth->remote_addr->sll_halen) {
		result = memcmp(conn_eth->remote_addr->sll_addr, addr->sll_addr,
				addr->sll_halen);
	} else {
		result = 1;
	}
	return result == 0;
}