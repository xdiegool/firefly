
#ifndef FIREFLY_TRANSPORT_ETH_POSIX_PRIVATE_H
#define FIREFLY_TRANSPORT_ETH_POSIX_PRIVATE_H


#include <transport/firefly_transport.h>
#include "transport/firefly_transport_private.h"
#include <transport/firefly_transport_eth_posix.h>
#include <signal.h>

#define FIREFLY_ETH_PROTOCOL	0x1337

struct transport_llp_eth_posix {
	int socket;
	struct firefly_event_queue *event_queue;
	firefly_on_conn_recv_eth_posix on_conn_recv;
};

struct protocol_connection_eth_posix {
	struct sockaddr_ll *remote_addr;
	struct firefly_transport_llp *llp;
	int socket;
};

struct firefly_event_llp_read_eth_posix {
	struct firefly_transport_llp *llp;
	struct sockaddr_ll *addr;
	unsigned char *data;
	size_t len;
};

int firefly_transport_eth_posix_read_event(void *event_arg);

int firefly_transport_llp_eth_posix_free_event(void *event_arg);

void firefly_transport_connection_eth_posix_free(struct firefly_connection *conn);

void firefly_transport_eth_posix_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn);

void get_mac_addr(struct sockaddr_ll *addr, char *mac_addr);

bool connection_eq_addr(struct firefly_connection *conn, void *context);

#endif
