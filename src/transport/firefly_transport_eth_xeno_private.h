
#ifndef FIREFLY_TRANSPORT_ETH_XENO_PRIVATE_H
#define FIREFLY_TRANSPORT_ETH_XENO_PRIVATE_H

#include <transport/firefly_transport.h>
#include "transport/firefly_transport_private.h"
#include <transport/firefly_transport_eth_xeno.h>
#include <netpacket/packet.h>	// defines sockaddr_ll
#include <signal.h>
#include <native/heap.h>

#define FIREFLY_ETH_PROTOCOL	0x1337

struct transport_llp_eth_xeno {
	int socket;
	struct firefly_event_queue *event_queue;
	firefly_on_conn_recv_eth_xeno on_conn_recv;
	RT_HEAP dyn_mem;
};

struct protocol_connection_eth_xeno {
	struct sockaddr_ll *remote_addr;
	struct firefly_transport_llp *llp;
	int socket;
};

struct firefly_event_llp_read_eth_xeno {
	struct firefly_transport_llp *llp;
	struct sockaddr_ll addr;
	unsigned char *data;
	size_t len;
};

int firefly_transport_eth_xeno_read_event(void *event_arg);

int firefly_transport_llp_eth_xeno_free_event(void *event_arg);

void firefly_transport_connection_eth_xeno_free(struct firefly_connection *conn);

void firefly_transport_eth_xeno_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id);

void firefly_transport_eth_xeno_ack(unsigned char pkt_id,
		struct firefly_connection *conn);

void get_mac_addr(struct sockaddr_ll *addr, char *mac_addr);

bool connection_eq_addr(struct firefly_connection *conn, void *context);

#endif
