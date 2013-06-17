#ifndef FIREFLY_TRANSPORT_ETH_STELLARIS_H
#define FIREFLY_TRANSPORT_ETH_STELLARIS_H

#include <transport/firefly_transport.h>

struct transport_llp_eth_stellaris;

struct protocol_connection_eth_stellaris;

void sendstuff();

typedef struct firefly_connection *(*firefly_on_conn_recv_eth_stellaris)(
		struct firefly_transport_llp *llp, unsigned char *mac_address);

/**
 * @brief Allocates and initializes a new \c firefly_transport_llp on the
 * Ethernet interface.
 *
 * @param on_conn_recv The callback to call when a new connection is received.
 *
 * @return A pointer to the created \c firefly_transport_llp.
 * @retval NULL Returns \c NULL upon failure.
 */
struct firefly_transport_llp *firefly_transport_llp_eth_stellaris_new(
		unsigned char *src_addr,
		struct firefly_event_queue *event_queue,
		firefly_on_conn_recv_eth_stellaris on_conn_recv);

/**
 * Frees a \c firefly_transport_llp and its members.
 *
 * @param llp The \c firefly_transport_llp to free
 */
void firefly_transport_llp_eth_stellaris_free(struct firefly_transport_llp *llp);

/**
 * Opens a new connection to the specified mac address with the specified
 * callbacks and \c firefly_event_queue.
 *
 * @param on_channel_opened The callback to call once a channel is opened.
 * @param on_channel_closed The callback to call once a channel is closed.
 * @param on_channel_recv The callback to call once a channel is received.
 * @param mac_address The MAC address to open a connection to.
 * @param llp The \c firefly_transport_llp to open a connection on.
 *
 * @return A pointer to the new \c firefly_connection that is opened.
 * @retval NULL Returns \c NULL upon failure.
 */
/* struct firefly_connection *firefly_transport_connection_eth_stellaris_open( */
/* 				firefly_channel_is_open_f on_channel_opened, */
/* 				firefly_channel_closed_f on_channel_closed, */
/* 				firefly_channel_accept_f on_channel_recv, */
/* 				unsigned char *mac_address, */
/* 				struct firefly_transport_llp *llp); */
struct firefly_connection *firefly_transport_connection_eth_stellaris_open(
		struct firefly_transport_llp *llp,
		unsigned char *mac_address,
		struct firefly_connection_actions *actions);

void firefly_transport_eth_stellaris_read(struct firefly_transport_llp *llp);

#endif
