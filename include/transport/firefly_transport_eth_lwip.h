#ifndef FIREFLY_TRANSPORT_ETH_LWIP_H
#define FIREFLY_TRANSPORT_ETH_LWIP_H

#include <transport/firefly_transport.h>

struct transport_llp_eth_lwip;

struct protocol_connection_eth_lwip;

typedef struct firefly_connection *(*firefly_on_conn_recv_eth_lwip)(
		struct firefly_transport_llp *llp, char *mac_address);

struct firefly_transport_llp *firefly_transport_llp_eth_lwip_new(
		const char *iface_name,
		firefly_on_conn_recv_eth_lwip on_conn_recv);

void firefly_transport_llp_eth_lwip_free(struct firefly_transport_llp **llp);

struct firefly_connection *firefly_transport_connection_eth_lwip_open(
				firefly_channel_is_open_f on_channel_opened,
				firefly_channel_closed_f on_channel_closed,
				firefly_channel_accept_f on_channel_recv,
				struct firefly_event_queue *event_queue,
				char *mac_address,
				char *if_name,
				struct firefly_transport_llp *llp);

void firefly_transport_connection_eth_lwip_close(struct firefly_connection *conn);

void firefly_transport_eth_lwip_read(struct firefly_transport_llp *llp);

#endif

