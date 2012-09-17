
#ifndef FIREFLY_TRANSPORT_ETH_POSIX_H
#define FIREFLY_TRANSPORT_ETH_POSIX_H

#include <transport/firefly_transport.h>

struct transport_llp_eth_posix;

typedef struct firefly_connection *(*firefly_on_conn_recv_eth_posix)(
		struct firefly_transport_llp *llp, char *mac_address);

struct firefly_transport_llp *firefly_transport_llp_eth_posix_new(
		const char *iface_name,
		firefly_on_conn_recv_eth_posix on_conn_recv);

void firefly_transport_llp_eth_posix_free(struct firefly_transport_llp **llp);

void firefly_transport_eth_posix_read(struct firefly_transport_llp *llp);

#endif
