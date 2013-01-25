
#ifndef FIREFLY_TRANSPORT_ETH_POSIX_H
#define FIREFLY_TRANSPORT_ETH_POSIX_H

#include <transport/firefly_transport.h>
#include <sys/time.h>

struct transport_llp_eth_posix;

struct protocol_connection_eth_posix;

typedef struct firefly_connection *(*firefly_on_conn_recv_eth_posix)(
		struct firefly_transport_llp *llp, char *mac_address);

struct firefly_transport_llp *firefly_transport_llp_eth_posix_new(
		const char *iface_name,
		firefly_on_conn_recv_eth_posix on_conn_recv,
		struct firefly_event_queue *event_queue);

void firefly_transport_llp_eth_posix_free(struct firefly_transport_llp *llp);

struct firefly_connection *firefly_transport_connection_eth_posix_open(
				firefly_channel_is_open_f on_channel_opened,
				firefly_channel_closed_f on_channel_closed,
				firefly_channel_accept_f on_channel_recv,
				char *mac_address,
				char *if_name,
				struct firefly_transport_llp *llp);

void firefly_transport_eth_posix_read(struct firefly_transport_llp *llp,
		struct timeval *tv);

#endif
