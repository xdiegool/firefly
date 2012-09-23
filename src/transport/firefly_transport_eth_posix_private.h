
#ifndef FIREFLY_TRANSPORT_ETH_POSIX_PRIVATE_H
#define FIREFLY_TRANSPORT_ETH_POSIX_PRIVATE_H

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
	sig_atomic_t open;
};

void recv_buf_resize(struct transport_llp_eth_posix *llp_eth, size_t new_size);

int connection_comp_addr(struct firefly_connection *conn, void *context);

typedef int (*cmp_conn_f)(struct firefly_connection *conn, void *context);

struct firefly_connection *find_connection(struct firefly_transport_llp *llp,
		void *context, cmp_conn_f cmp_conn);

#endif
