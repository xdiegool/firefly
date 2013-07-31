#ifndef FIREFLY_TRANSPORT_ETH_STELLARIS_H
#define FIREFLY_TRANSPORT_ETH_STELLARIS_H

#include <transport/firefly_transport.h>

/**
 * @brief This callback will be called when a new connection is received.
 *
 * This function is implemented by the application layer. It will be called
 * when the transport layer receives a new connection. It will be called with
 * address of the remote node of the connection as argument. The application
 * layer may open a new connection to this remote node on its own.
 * Currently:
 * If a connection is opened, true must be returned. If so the read event
 * calling this function will be rescheduled. If no new connection was opened to
 * the supplied addres the data is discarded.
 *
 * Future: TODO
 * If a connection is opened, the id of the event as returned by
 * #firefly_connection_open must be returned. If no new connection is
 * opened 0 must be returned.
 *
 * @param llp The \a llp the incomming connection is associated with.
 * @param mac_address The MAC addr of the remote node.
 * Currently:
 * @return \c bool Indicating if a connection was opened to the address or not.
 * @retval true if a new connection was opened.
 * @retval false if no new connection was opened.
 *
 * Future: TODO
 * @return Event id or 0.
 * @retval >0 A new connection was opened and the read data will propagate as
 * soon as the connection is completely open.
 * @retval 0 The new connection was refused and the read data is discarded.
 */
typedef bool (*firefly_on_conn_recv_eth_stellaris)(
		struct firefly_transport_llp *llp, unsigned char *mac_address);

/**
 * @brief Allocates and initializes a new \c firefly_transport_llp on the
 * Ethernet interface.
 *
 * @param src_addr The source address to set as is in each packet sent. This
 * must be formatted correctly to be sent in an ethernet packet as is.
 * @param event_queue The event queue to push spawned events to.
 * @param on_conn_recv The callback to call when a new connection is received.
 *
 * @return A pointer to the created \c struct #firefly_transport_llp.
 * @retval NULL on failure.
 */
struct firefly_transport_llp *firefly_transport_llp_eth_stellaris_new(
		unsigned char *src_addr,
		struct firefly_event_queue *event_queue,
		firefly_on_conn_recv_eth_stellaris on_conn_recv);

/**
 * @brief Frees a \c firefly_transport_llp and its members. Must have been
 * created with #firefly_transport_llp_eth_stellaris_new().
 *
 * @param llp The \c struct #firefly_transport_llp to free
 */
void firefly_transport_llp_eth_stellaris_free(struct firefly_transport_llp *llp);

/**
 * @brief Allocates and initializes the transport layer specific data of a
 * #firefly_connection. It shall be supplied as parameter to
 * #firefly_connection_open().
 *
 * @param llp The \c #firefly_transport_llp to associate the data with.
 * @param mac_address The MAC address of the remote node.
 * @return The transport specific data ready to be supplied as argument to
 * #firefly_connection_open().
 * @retval NULL upon failure.
 * @see #firefly_connection_open()
 */
struct firefly_transport_connection *
firefly_transport_connection_eth_stellaris_new(
		struct firefly_transport_llp *llp,
		unsigned char *mac_address);

/**
 * @brief Read data from the #firefly_transport_llp. Any read data will be
 * included in an event pushed to the #firefly_event_queue.
 *
 * The read data will be distributed to the connection opened to the remote
 * address the data is sent from.
 *
 * If no such connection exists the #firefly_on_conn_recv_eth_stellaris() will
 * be called, if it is NULL the data will be discarded.
 *
 * This function is blocking.
 *
 * @param llp The Link Layer Port to read data from.
 * @see #firefly_on_conn_recv_eth_stellaris()
 */
void firefly_transport_eth_stellaris_read(struct firefly_transport_llp *llp);

#endif
