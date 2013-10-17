/**
 * @file
 * @brief The public API of the transport Ethernet POSIX with specific structures and
 * functions.
 */
#define _POSIX_C_SOURCE (200112L) // Needed to define strerror_r().
#include <pthread.h>

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
#include <arpa/inet.h>		// defines htons
#include <linux/if.h>		// defines ifreq, IFNAMSIZ

#include <utils/firefly_event_queue.h>
#include <utils/firefly_errors.h>
#include <transport/firefly_transport.h>

#include <utils/firefly_resend_posix.h>
#include "utils/firefly_event_queue_private.h"
#include "transport/firefly_transport_private.h"
#include "protocol/firefly_protocol_private.h"
#include "utils/cppmacros.h"

#define ERROR_STR_MAX_LEN      (256)

struct firefly_transport_llp *firefly_transport_llp_eth_posix_new(
		const char *iface_name,
		firefly_on_conn_recv_eth_posix_f on_conn_recv,
		struct firefly_event_queue *event_queue)
{
	int err;
	struct ifreq ifr;
	struct sockaddr_ll addr;
	struct transport_llp_eth_posix *llp_eth;
	struct firefly_transport_llp *llp;
	llp_eth = malloc(sizeof(*llp_eth));

	if (!llp_eth) {
		FFL(FIREFLY_ERROR_ALLOC);
		return NULL;
	}
	/* Create the socket */
	err = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL));
	if (err < 0) {
		FFL(FIREFLY_ERROR_SOCKET);
		return NULL;
	}
	llp_eth->socket = err;
	strncpy(ifr.ifr_name, iface_name, IFNAMSIZ);
	/* Retreive the interface index of the interface and save it to
	* ifr.ifr_ifindex. */
	err = ioctl(llp_eth->socket, SIOCGIFINDEX, &ifr);
	if(err < 0){
		close(llp_eth->socket);
		free(llp_eth);
		FFL(FIREFLY_ERROR_SOCKET);
		return NULL;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sll_family   = AF_PACKET;
	addr.sll_protocol = htons(FIREFLY_ETH_PROTOCOL);
	addr.sll_ifindex  = ifr.ifr_ifindex;
	/* Retreive the mac address of the interface and save it to ifr */
	err = ioctl(llp_eth->socket, SIOCGIFHWADDR, &ifr);
	if(err < 0){
		close(llp_eth->socket);
		free(llp_eth);
		FFL(FIREFLY_ERROR_SOCKET);
		return NULL;
	}
	memcpy(ifr.ifr_hwaddr.sa_data, addr.sll_addr, 6);
	addr.sll_halen = 6;
	/* Bind socket to specified interface */
	err = bind(llp_eth->socket, (struct sockaddr *)&addr,
		    sizeof(struct sockaddr_ll));
	if(err < 0){
		close(llp_eth->socket);
		free(llp_eth);
		FFL(FIREFLY_ERROR_SOCKET);
		return NULL;
	}

	llp_eth->on_conn_recv		= on_conn_recv;
	llp_eth->event_queue		= event_queue;
	llp_eth->resend_queue = firefly_resend_queue_new();
	memset(&llp_eth->read_thread, 0, sizeof(llp_eth->read_thread));
	memset(&llp_eth->resend_thread, 0, sizeof(llp_eth->read_thread));
	llp_eth->running = false;

	llp				= malloc(sizeof(*llp));
	if (!llp) {
		close(llp_eth->socket);
		free(llp_eth);
		FFL(FIREFLY_ERROR_ALLOC);
		return NULL;
	}
	llp->llp_platspec		= llp_eth;
	llp->conn_list			= NULL;
	llp->protocol_data_received_cb	= protocol_data_received;
	llp->state				= FIREFLY_LLP_OPEN;
	return llp;
}

void firefly_transport_llp_eth_posix_free(struct firefly_transport_llp *llp)
{
	struct transport_llp_eth_posix *llp_eth;
	llp_eth = (struct transport_llp_eth_posix *) llp->llp_platspec;
	int ret = llp_eth->event_queue->offer_event_cb(llp_eth->event_queue,
			FIREFLY_PRIORITY_LOW, firefly_transport_llp_eth_posix_free_event,
			llp, 0, NULL);
	FFLIF(ret < 0, FIREFLY_ERROR_ALLOC);
}

static void check_llp_free(struct firefly_transport_llp *llp)
{
	struct transport_llp_eth_posix *llp_eth;
	if (llp->state == FIREFLY_LLP_CLOSING && llp->conn_list == NULL) {
		llp_eth = llp->llp_platspec;
		close(llp_eth->socket);
		firefly_resend_queue_free(llp_eth->resend_queue);
		free(llp_eth);
		free(llp);
	}
}

int firefly_transport_llp_eth_posix_free_event(void *event_arg)
{
	struct firefly_transport_llp *llp;
	llp = event_arg;

	llp->state = FIREFLY_LLP_CLOSING;

	// Close all connections.
	struct llp_connection_list_node *head = llp->conn_list;
	while (head != NULL) {
		firefly_connection_close(head->conn);
		head = head->next;
	}
	check_llp_free(llp);
	return 0;
}

static int connection_open(struct firefly_connection *conn)
{
	struct firefly_transport_connection_eth_posix *tcep;
	tcep = conn->transport->context;
	add_connection_to_llp(conn, tcep->llp);
	return 0;
}

static int connection_close(struct firefly_connection *conn)
{
	struct firefly_transport_connection_eth_posix *tcep;
	struct firefly_transport_llp *llp;
	tcep = conn->transport->context;
	llp = tcep->llp;

	remove_connection_from_llp(tcep->llp, conn,
			firefly_connection_eq_ptr);
	free(tcep->remote_addr);
	free(tcep);
	free(conn->transport);
	check_llp_free(llp);
	return 0;
}

struct firefly_transport_connection *firefly_transport_connection_eth_posix_new(
		struct firefly_transport_llp *llp,
		char *mac_address,
		char *if_name)

{
	int err;
	struct firefly_transport_connection *tc;
	struct firefly_transport_connection_eth_posix *tcep;
	struct transport_llp_eth_posix * llp_eth;
	struct ifreq ifr;
	llp_eth = llp->llp_platspec;
	/* Get ifindex first to avoid mem alloc if it fails. */
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	err = ioctl(((struct transport_llp_eth_posix *)llp->llp_platspec)->socket,
			SIOCGIFINDEX, &ifr);
	if(err < 0) {
		FFL(FIREFLY_ERROR_SOCKET);
		return NULL;
	}

	tcep = malloc(sizeof(*tcep));
	tc = malloc(sizeof(*tc));
	if (tc == NULL || tcep == NULL) {
		free(tc);
		free(tcep);
		return NULL;
	}

	/* Construct address  { */
	tcep->remote_addr = malloc(sizeof(struct sockaddr_ll));
	if (tcep->remote_addr == NULL) {
		free(tc);
		free(tcep);
		FFL(FIREFLY_ERROR_ALLOC);
		return NULL;
	}
	memset(tcep->remote_addr, 0, sizeof(struct sockaddr_ll));
	tcep->remote_addr->sll_family   = AF_PACKET;
	tcep->remote_addr->sll_protocol = htons(FIREFLY_ETH_PROTOCOL);
	/* Convert address to to binary data */
	ether_aton_r(mac_address, (void*)&tcep->remote_addr->sll_addr);
	tcep->remote_addr->sll_halen    = 6;
	tcep->remote_addr->sll_ifindex  = ifr.ifr_ifindex;
	/* } */

	tcep->socket = llp_eth->socket;
	tcep->llp = llp;
	tcep->timeout = FIREFLY_TRANSPORT_ETH_POSIX_DEFAULT_TIMEOUT;
	tc->context = tcep;
	tc->open = connection_open;
	tc->close = connection_close;
	tc->write = firefly_transport_eth_posix_write;
	tc->ack = firefly_transport_eth_posix_ack;

	return tc;
}

void firefly_transport_eth_posix_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id)
{
	int err;
	struct firefly_transport_connection_eth_posix *tcep =
		 conn->transport->context;
	err = sendto(tcep->socket, data, data_size, 0,
			(struct sockaddr *)tcep->remote_addr,
			sizeof(*tcep->remote_addr));
	if (err < 0) {
		FFL(FIREFLY_ERROR_SOCKET);
		firefly_connection_raise_later(conn,
				FIREFLY_ERROR_TRANS_WRITE, "sendto() failed");
	}
	if (important && id != NULL) {
		unsigned char *new_data;
		struct transport_llp_eth_posix *llp_ps;

		new_data = malloc(data_size);
		if (!new_data) {
			FFL(FIREFLY_ERROR_ALLOC);
			return;
		}
		memcpy(new_data, data, data_size);
		llp_ps = tcep->llp->llp_platspec;
		*id = firefly_resend_add(llp_ps->resend_queue,
				new_data, data_size, tcep->timeout,
				FIREFLY_TRANSPORT_ETH_POSIX_DEFAULT_RETRIES, conn);
	}
}

void firefly_transport_eth_posix_ack(unsigned char pkt_id,
		struct firefly_connection *conn)
{
	struct firefly_transport_connection_eth_posix *conn_eth;
	struct transport_llp_eth_posix *llpep;

	conn_eth = conn->transport->context;
	llpep = conn_eth->llp->llp_platspec;
	firefly_resend_remove(llpep->resend_queue, pkt_id);
}

struct firefly_event_llp_read_eth_posix {
	struct firefly_transport_llp *llp;
	struct sockaddr_ll addr;
	size_t len;
	unsigned char *data;
};

static int firefly_transport_eth_posix_read_event(void *event_args)
{
	struct firefly_event_llp_read_eth_posix *ev_a;
	struct transport_llp_eth_posix *llp_eth;
	struct firefly_connection *conn;

	ev_a = event_args;
	llp_eth = ev_a->llp->llp_platspec;
	conn = find_connection(ev_a->llp, &ev_a->addr, connection_eq_addr);
	if (conn == NULL) {
		char mac_addr[MACADDR_STRLEN];
		get_mac_addr(&ev_a->addr, mac_addr);
		int64_t ev_id = 0;
		if (llp_eth->on_conn_recv != NULL &&
				(ev_id = llp_eth->on_conn_recv(ev_a->llp, mac_addr)) > 0) {
			/* Connection accepted; reschedule event. */
			return llp_eth->event_queue->offer_event_cb(
					llp_eth->event_queue,
					FIREFLY_PRIORITY_HIGH,
					firefly_transport_eth_posix_read_event,
					ev_a, 1, &ev_id);
		} else {
			free(ev_a->data);
		}
	} else {
		ev_a->llp->protocol_data_received_cb(conn, ev_a->data, ev_a->len);
	}
	free(ev_a);

	return 0;
}

void firefly_transport_eth_posix_read(struct firefly_transport_llp *llp,
		struct timeval *tv)
{
	struct firefly_event_llp_read_eth_posix *ev_arg;
	struct transport_llp_eth_posix *llp_eth;
	socklen_t addr_len;
	unsigned char tmp_buffer[1500];
	struct sockaddr_ll tmp_address;
	fd_set fs;
	int res;

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

	addr_len = sizeof(tmp_address);
	res = recvfrom(llp_eth->socket, tmp_buffer, 1500, MSG_DONTWAIT,
			(struct sockaddr *) &tmp_address, &addr_len);
	if (res == -EWOULDBLOCK || res == -EAGAIN) {
		return;
	} else if (res < 0) {
		char err_buf[ERROR_STR_MAX_LEN];
		strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
			      "recvfrom() failed in %s.\n%s()\n",
			      __FUNCTION__, err_buf);
		return;
	}
	ev_arg = malloc(sizeof(*ev_arg));
	if (!ev_arg) {
		FFL(FIREFLY_ERROR_ALLOC);
		return;
	}
	ev_arg->data = malloc(res);
	if (!ev_arg->data) {
		FFL(FIREFLY_ERROR_ALLOC);
		return;
	}
	ev_arg->len = res;
	ev_arg->addr = tmp_address;
	ev_arg->llp = llp;
	memcpy(ev_arg->data, tmp_buffer, ev_arg->len);

	llp_eth->event_queue->offer_event_cb(llp_eth->event_queue,
			FIREFLY_PRIORITY_HIGH,
			firefly_transport_eth_posix_read_event, ev_arg, 0, NULL);
}

void *firefly_transport_eth_posix_read_run(void *arg)
{
	struct firefly_transport_llp *llp;
	struct transport_llp_eth_posix *llp_eth;
	struct timeval tv = {
		.tv_sec = 0,
		.tv_usec = FIREFLY_TRANSPORT_ETH_POSIX_DEFAULT_TIMEOUT * 1000
	};

	llp = arg;
	llp_eth = llp->llp_platspec;
	while (llp_eth->running) {
		firefly_transport_eth_posix_read(llp, &tv);
	}
	return NULL;
}

static void resend_on_no_ack(struct firefly_connection *conn)
{
	firefly_connection_raise_later(conn, FIREFLY_ERROR_TRANS_WRITE, NULL);
}

int firefly_transport_eth_posix_run(struct firefly_transport_llp *llp)
{
	int res;
	struct transport_llp_eth_posix *llp_eth;
	struct firefly_resend_loop_args *largs;

	llp_eth = llp->llp_platspec;
	llp_eth->running = true;
	res = pthread_create(&llp_eth->read_thread, NULL,
			firefly_transport_eth_posix_read_run, llp);
	if (res < 0)
		return res;
	largs = malloc(sizeof(*largs));
	if (!largs) {
		llp_eth->running = false;
		return -1;
	}
	largs->rq = llp_eth->resend_queue;
	largs->on_no_ack = resend_on_no_ack;
	res = pthread_create(&llp_eth->resend_thread, NULL,
				 firefly_resend_run, largs);
	if (res < 0) {
		llp_eth->running = false;
		return res;
	}
	return 0;
}

int firefly_transport_eth_posix_stop(struct firefly_transport_llp *llp)
{
	struct transport_llp_eth_posix *llp_eth;
	llp_eth = llp->llp_platspec;
	llp_eth->running = false;
	pthread_cancel(llp_eth->resend_thread);
	pthread_join(llp_eth->resend_thread, NULL);
	pthread_join(llp_eth->read_thread, NULL);
	return 0;
}

void get_mac_addr(struct sockaddr_ll *addr, char *mac_addr)
{
	char temp_addr[MACADDR_STRLEN];
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
	struct firefly_transport_connection_eth_posix *conn_eth;
	struct sockaddr_ll *addr;
	int result;

	conn_eth = conn->transport->context;
	addr = context;

	if (addr->sll_halen == conn_eth->remote_addr->sll_halen) {
		result = memcmp(conn_eth->remote_addr->sll_addr, addr->sll_addr,
				addr->sll_halen);
	} else {
		result = 1;
	}
	return result == 0;
}
