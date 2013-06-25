#ifndef FIREFLY_TRANSPORT_ETH_XENO_H
#define FIREFLY_TRANSPORT_ETH_XENO_H

#include <transport/firefly_transport.h>
#include <sys/time.h>

struct transport_llp_eth_xeno;

struct protocol_connection_eth_xeno;

typedef struct firefly_connection *(*firefly_on_conn_recv_eth_xeno)(
		struct firefly_transport_llp *llp, char *mac_address);

struct firefly_transport_llp *firefly_transport_llp_eth_xeno_new(
		const char *iface_name,
		firefly_on_conn_recv_eth_xeno on_conn_recv,
		struct firefly_event_queue *event_queue);

void firefly_transport_llp_eth_xeno_free(struct firefly_transport_llp *llp);

struct firefly_connection *firefly_transport_connection_eth_xeno_open(
		struct firefly_transport_llp *llp,
		char *mac_address,
		char *if_name,
		struct firefly_connection_actions *actions);

void firefly_transport_eth_xeno_read(struct firefly_transport_llp *llp,
		int64_t *timeout);

#endif
