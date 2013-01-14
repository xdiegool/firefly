#ifndef FIREFLY_TRANSPORT_ETH_STELLARIS_PRIVATE_H
#define FIREFLY_TRANSPORT_ETH_STELLARIS_PRIVATE_H


#include <transport/firefly_transport.h>
#include "transport/firefly_transport_private.h"
#include <transport/firefly_transport_eth_stellaris.h>
#include <signal.h>

#include <lwip/pbuf.h>  // For struct pbuf.
#include <lwip/netif.h> // For struct netif.

#define FIREFLY_ETH_PROTOCOL	0x1337

struct transport_llp_eth_stellaris {
	struct netif *interface;
	struct pbuf *recv_buf; // TODO: Can we keep it here?
	size_t recv_buf_size;
	firefly_on_conn_recv_eth_stellaris on_conn_recv;
};

struct protocol_connection_eth_stellaris {
	struct eth_addr* remote_addr;
	sig_atomic_t open; /**< The flag indicating the opened state of a
						 connection.*/
};

//struct firefly_connection *firefly_transport_connection_eth_stellaris_new(
		//struct firefly_transport_llp *llp, char *mac_address);

int firefly_transport_connection_eth_stellaris_free_event(void *event_arg);

void firefly_transport_connection_eth_stellaris_free(struct firefly_connection *conn);

void firefly_transport_eth_stellaris_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn);

// void recv_buf_resize(struct transport_llp_eth_stellaris *llp_eth, size_t new_size);

// void get_mac_addr(struct sockaddr_ll *addr, char *mac_addr);

// bool connection_eq_addr(struct firefly_connection *conn, void *context);

#endif
