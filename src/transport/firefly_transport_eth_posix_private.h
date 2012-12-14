
#ifndef FIREFLY_TRANSPORT_ETH_POSIX_PRIVATE_H
#define FIREFLY_TRANSPORT_ETH_POSIX_PRIVATE_H


#include <transport/firefly_transport.h>
#include "transport/firefly_transport_private.h"
#include <transport/firefly_transport_eth_posix.h>
#include <signal.h>

#define FIREFLY_ETH_PROTOCOL	0x1337

struct transport_llp_eth_posix {
	int socket;
	unsigned char *recv_buf;
	size_t recv_buf_size;
	firefly_on_conn_recv_eth_posix on_conn_recv;
};

struct protocol_connection_eth_posix {
	struct sockaddr_ll *remote_addr;
	int socket;
	sig_atomic_t open; /**< The flag indicating the opened state of a
						 connection.*/
};

//struct firefly_connection *firefly_transport_connection_eth_posix_new(
		//struct firefly_transport_llp *llp, char *mac_address);

int firefly_transport_connection_eth_posix_free_event(void *event_arg);

void firefly_transport_connection_eth_posix_free(struct firefly_connection *conn);

void firefly_transport_eth_posix_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn);

void recv_buf_resize(struct transport_llp_eth_posix *llp_eth, size_t new_size);

void get_mac_addr(struct sockaddr_ll *addr, char *mac_addr);

bool connection_eq_addr(struct firefly_connection *conn, void *context);

#endif
