/**
 * @file
 * @brief The public API of the transport Ethernet POSIX with specific structures and
 * functions.
 */
#define _POSIX_C_SOURCE (200112L)

#include "transport/firefly_transport_eth_posix_private.h"
#include <transport/firefly_transport_eth_posix.h>

#include <stdlib.h>
#include <stdio.h>
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
#include "utils/cppmacros.h"

#define ERROR_STR_MAX_LEN      (256)
#ifdef __XENO__
#define XENO_HEAP_SIZE			(1024)
#endif

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
	FFLIF((err < 0), FIREFLY_ERROR_SOCKET);

	llp_eth->socket = err;

	/* Set the interface name */
	strncpy(ifr.ifr_name, iface_name, IFNAMSIZ);

	/* Retreive the interface index of the interface and save it to
	* ifr.ifr_ifindex. */
	err = ioctl(llp_eth->socket, SIOCGIFINDEX, &ifr);
	if(err < 0){
		close(llp_eth->socket);
		FFL(FIREFLY_ERROR_SOCKET);
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
	err = ioctl(llp_eth->socket, SIOCGIFHWADDR, &ifr);
	if(err < 0){
		close(llp_eth->socket);
		FFL(FIREFLY_ERROR_SOCKET);
	}
	/* 6 is the length of a mac address */
	memcpy(ifr.ifr_hwaddr.sa_data, addr.sll_addr, 6);
	addr.sll_halen = 6;
	/* Bind socket to specified interface */
	err = bind(llp_eth->socket, (struct sockaddr *)&addr,
		    sizeof(struct sockaddr_ll));
	if(err < 0){
		close(llp_eth->socket);
		FFL(FIREFLY_ERROR_SOCKET);
	}

	llp_eth->event_queue		= event_queue;
	llp_eth->on_conn_recv		= on_conn_recv;

	llp				= malloc(sizeof(*llp));
	llp->llp_platspec		= llp_eth;
	llp->conn_list			= NULL;
	llp->protocol_data_received_cb	= protocol_data_received;
#ifdef __XENO__
	err = rt_heap_create(&llp_eth->dyn_mem, "somename", XENO_HEAP_SIZE, H_MAPPABLE);
	if (err) {
		switch (err) {
			case -EPERM:
				fprintf(stderr, "Invalid context\n");
				break;
			case -ENOSYS:
				fprintf(stderr, "Mappable but missing kernel support\n");
				break;
			case -EINVAL:
				fprintf(stderr, "Invalid argument\n");
				break;
		}
	}
	FFLIF(err, FIREFLY_ERROR_ALLOC);
#endif

	return llp;
}

void firefly_transport_llp_eth_posix_free(struct firefly_transport_llp *llp)
{
	struct transport_llp_eth_posix *llp_eth;
	llp_eth = (struct transport_llp_eth_posix *) llp->llp_platspec;
	int ret = llp_eth->event_queue->offer_event_cb(llp_eth->event_queue,
			FIREFLY_PRIORITY_LOW, firefly_transport_llp_eth_posix_free_event, llp);
	FFLIF(ret, FIREFLY_ERROR_ALLOC);
}

int firefly_transport_llp_eth_posix_free_event(void *event_arg)
{
	struct firefly_transport_llp *llp;
	struct transport_llp_eth_posix *llp_eth;

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
#ifdef __XENO__
		rt_heap_delete(&llp_eth->dyn_mem);
#endif
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
	conn_eth = conn->transport_conn_platspec;

	remove_connection_from_llp(conn_eth->llp, conn,
			firefly_connection_eq_ptr);
	free(conn_eth->remote_addr);
	free(conn_eth);
}
#ifdef __XENO__
static void *xeno_mem_alloc(struct firefly_connection *conn, size_t size)
{
	void *buf;
	struct protocol_connection_eth_posix *conn_eth;
	conn_eth = conn->transport_conn_platspec;
	struct transport_llp_eth_posix *llp_eth;
	llp_eth = conn_eth->llp->llp_platspec;
	int err = rt_heap_alloc(&llp_eth->dyn_mem, size, TM_NONBLOCK, &buf);
	if (err) {
		FFL(FIREFLY_ERROR_ALLOC);
		return NULL;
	} else {
		return buf;
	}
}

static void xeno_mem_free(struct firefly_connection *conn, void *ptr)
{
	struct protocol_connection_eth_posix *conn_eth;
	conn_eth = conn->transport_conn_platspec;
	struct transport_llp_eth_posix *llp_eth;
	llp_eth = conn_eth->llp->llp_platspec;
	int err = rt_heap_free(&llp_eth->dyn_mem, ptr);
	FFLIF(err, FIREFLY_ERROR_ALLOC);
}

static struct firefly_memory_funcs conn_xeno_mem_fs = {
	.alloc_replacement = xeno_mem_alloc,
	.free_replacement = xeno_mem_free
};
#endif

struct firefly_connection *firefly_transport_connection_eth_posix_open(
		struct firefly_transport_llp *llp,
		char *mac_address,
		char *if_name,
		struct firefly_connection_actions *actions)

{
	/* Get ifindex first to avoid mem alloc if it fails. */
	int err;
	struct ifreq ifr;
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	err = ioctl(((struct transport_llp_eth_posix *)llp->llp_platspec)->socket,
			SIOCGIFINDEX, &ifr);
	if(err < 0) {
		FFL(FIREFLY_ERROR_SOCKET);
		return NULL;
	}

	/* Alloc connection structs */
	struct protocol_connection_eth_posix *conn_eth;
	conn_eth = malloc(sizeof(*conn_eth));
	struct firefly_connection *conn = firefly_connection_new(
			actions,
			firefly_transport_eth_posix_write,
			firefly_transport_eth_posix_ack,
#ifdef __XENO__
			&conn_xeno_mem_fs,
#else
			NULL,
#endif
			((struct transport_llp_eth_posix *)llp->llp_platspec)->event_queue,
			conn_eth, firefly_transport_connection_eth_posix_free);
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
		((struct transport_llp_eth_posix *)llp->llp_platspec)->socket;

	conn_eth->llp = llp;
	add_connection_to_llp(conn, llp);

	return conn;
}

void firefly_transport_eth_posix_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id)
{
	int err;
	struct protocol_connection_eth_posix *conn_eth =
		(struct protocol_connection_eth_posix *) conn->transport_conn_platspec;
	err = sendto(conn_eth->socket, data, data_size, 0,
			(struct sockaddr *)conn_eth->remote_addr,
			sizeof(*conn_eth->remote_addr));
	if (err == -1) {
		FFL(FIREFLY_ERROR_SOCKET);
		if (conn->actions->connection_error)
			conn->actions->connection_error(conn);
	}
	if (important && id != NULL) {
		// TODO
	}
}

void firefly_transport_eth_posix_ack(unsigned char pkt_id,
		struct firefly_connection *conn)
{
	UNUSED_VAR(pkt_id);
	UNUSED_VAR(conn);
}

void firefly_transport_eth_posix_read(struct firefly_transport_llp *llp,
		struct timeval *tv)
{
	int res;
	struct transport_llp_eth_posix *llp_eth;
	unsigned char tmp_buffer[1500];
	fd_set fs;

	llp_eth = llp->llp_platspec;
	FD_ZERO(&fs);
	FD_SET(llp_eth->socket, &fs);
	res = select(llp_eth->socket + 1, &fs, NULL, NULL, tv);
	if (res == 0)
		return;
	if (res == -1) {
		FFL(FIREFLY_ERROR_SOCKET);
		return;
	}

	struct firefly_event_llp_read_eth_posix *ev_arg = malloc(sizeof(*ev_arg));
	FFLIF(!ev_arg, FIREFLY_ERROR_ALLOC);
	ev_arg->addr = malloc(sizeof(struct sockaddr_ll));
	FFLIF(!ev_arg->addr, FIREFLY_ERROR_ALLOC);

	ev_arg->len = 0;
	ev_arg->data = NULL;
	ev_arg->llp = llp;

	// Read data from socket, = 0 is crucial due to ioctl only sets the
	// first 32 bits of pkg_len
	/*res = ioctl(llp_eth->socket, FIONREAD, &ev_arg->len);*/
	/*if (res == -1) {*/
		/*FFL(FIREFLY_ERROR_SOCKET);*/
		/*ev_arg->len = 0;*/
		/*return;*/
	/*}*/

	socklen_t addr_len = sizeof(*ev_arg->addr);
	res = recvfrom(llp_eth->socket, tmp_buffer, 1500, 0,
			(struct sockaddr *) ev_arg->addr, &addr_len);
	if (res == -1) {
		char err_buf[ERROR_STR_MAX_LEN];
		strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
			      "recvfrom() failed in %s.\n%s()\n",
			      __FUNCTION__, err_buf);
		free(ev_arg->addr);
		free(ev_arg);
		return;
	}

	ev_arg->len = res;
	ev_arg->data = malloc(ev_arg->len);
	FFLIF(!ev_arg->data, FIREFLY_ERROR_ALLOC);
	if (!ev_arg->data) {
		return;
	}
	memcpy(ev_arg->data, tmp_buffer, ev_arg->len);

	llp_eth->event_queue->offer_event_cb(llp_eth->event_queue,
			FIREFLY_PRIORITY_HIGH,
			firefly_transport_eth_posix_read_event, ev_arg);
}

int firefly_transport_eth_posix_read_event(void *event_args)
{
	struct firefly_event_llp_read_eth_posix *ev_a = event_args;
	struct transport_llp_eth_posix *llp_eth = ev_a->llp->llp_platspec;

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
	struct protocol_connection_eth_posix *conn_eth;
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
