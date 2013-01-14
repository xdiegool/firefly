#ifndef FIREFLY_TRANSPORT_ETH_STELLARIS_H
#define FIREFLY_TRANSPORT_ETH_STELLARIS_H

#include <transport/firefly_transport.h>

#include <lwip/pbuf.h>  // For struct pbuf.
#include <lwip/netif.h> // For struct netif.

struct transport_llp_eth_stellaris;

struct protocol_connection_eth_stellaris;

typedef struct firefly_connection *(*firefly_on_conn_recv_eth_stellaris)(
		struct firefly_transport_llp *llp, char *mac_address);

/**
 * @brief Allocates and initializes a new \c firefly_transport_llp on an
 * Ethernet interface.
 *
 * @param iface_num The number of the interface to listen to.
 * @param on_conn_recv The callback to call when a new connection is received.
 *
 * @return A pointer to the created \c firefly_transport_llp.
 * @retval NULL Returns \c NULL upon failure.
 */
struct firefly_transport_llp *firefly_transport_llp_eth_stellaris_new(int iface_num,
		firefly_on_conn_recv_eth_stellaris on_conn_recv);

/**
 * Callback used by stellaris to signal that a new Ethernet frame was received.
 *
 * @warning Should not be altered! It's an ugly hack to make stellaris play nicely
 * with us...
 *
 * @param netif The network interface the freame was received on.
 * @param pbuf The data that was received.
 */
void firefly_recieve_ethernet(struct netif *netif, struct pbuf *pbuf);

/**
 * Frees a \c firefly_transport_llp and its members.
 *
 * @param llp The \c firefly_transport_llp to free
 */
void firefly_transport_llp_eth_stellaris_free(struct firefly_transport_llp **llp);

/**
 * Opens a new connection to the specified mac address with the specified
 * callbacks and \c firefly_event_queue.
 *
 * @param on_channel_opened The callback to call once a channel is opened.
 * @param on_channel_closed The callback to call once a channel is closed.
 * @param on_channel_recv The callback to call once a channel is received.
 * @param event_queue The \c firefly_event_queue to be used for this connection.
 * @param mac_address The MAC address to open a connection to.
 * @param llp The \c firefly_transport_llp to open a connection on.
 *
 * @return A pointer to the new \c firefly_connection that is opened.
 * @retval NULL Returns \c NULL upon failure.
 */
struct firefly_connection *firefly_transport_connection_eth_stellaris_open(
				firefly_channel_is_open_f on_channel_opened,
				firefly_channel_closed_f on_channel_closed,
				firefly_channel_accept_f on_channel_recv,
				struct firefly_event_queue *event_queue,
				char *mac_address,
				struct firefly_transport_llp *llp);

/**
 * Closes the provided connection.
 *
 * @param conn The connection to close.
 */
void firefly_transport_connection_eth_stellaris_close(struct firefly_connection *conn);

// TODO: Do we need this?
void firefly_transport_eth_stellaris_read(struct firefly_transport_llp *llp);

#endif

