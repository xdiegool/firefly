/**
 * @file
 * @brief The public API of the transport UDP LWIP with specific structures and
 * functions.
 */
#ifndef FIREFLY_TRANSPORT_UDP_LWIP_H
#define FIREFLY_TRANSPORT_UDP_LWIP_H

#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport.h>

#include "utils/firefly_event_queue.h"

/**
 * @brief This callback will be called when a new connection is received.
 *
 * This function is implemented by the application layer. It will be
 * called when the transport layer receives a new connection. It will be
 * called with the llp the connection was received on and the address
 * and port of the remote node of the connection as argument. The
 * application layer may open a new connection to this remote node by
 * calling #firefly_connection_open() with the result of calling
 * #firefly_transport_connection_udp_lwip_new() as an argument. If the
 * connection is opened true must be returned, otherwise false should be
 * returned.
 *
 * If this callback is not implemented (i.e. the pointer is NULL),
 * connections will automatically be denied.
 *
 * @param llp The LLP on which the connection was received.
 * @param ip_addr The IP addr of the remote node.
 * @param port The port number of the remote node.
 * @return A boolean indicating whether the connection is accepted or
 * not.
 * @retval true If connection was opened.
 * @retval false If no connection was opened.
 */
typedef bool (*firefly_on_conn_recv_udp_lwip_f)(
		struct firefly_transport_llp *llp, char *ip_addr, unsigned short port);

/**
 * @brief Allocates and initializes a new  #firefly_transport_llp with
 * UDP specific data and opens an UDP socket bound to the specified \a
 * local_port.
 *
 * @param local_ip_addr The local IP address to bind to. Can be IP_ADDR_ANY to
 * bind to all known interfaces.
 * @param local_port The port to bind the new socket to.
 * @param on_conn_recv The callback to call when a new connection is received
 * @param event_queue The event queue to associate with this
 * #firefly_transport_llp.
 * @return A pointer to the created #firefly_transport_llp.
 */
struct firefly_transport_llp *firefly_transport_llp_udp_lwip_new(
		char *local_ip_addr, unsigned short local_port,
		firefly_on_conn_recv_udp_lwip_f on_conn_recv,
		struct firefly_event_queue *event_queue);

/**
 * @brief Spawns an event that closes the socket and frees any resources
 * associated with this #firefly_transport_llp.
 *
 * The event will call #firefly_connection_close which closes all
 * channels and then the connection. If all connections are closed, it
 * will free the LLP as well. Otherwise, it calls this functions again
 * to give all connections time to close.
 *
 * @param llp The firefly_transport_llp to free.
 */
void firefly_transport_llp_udp_lwip_free(struct firefly_transport_llp *llp);

/**
 * @brief Called when opening a connection on the provided link layer
 * port. The function sets up all the needed transport layer specific
 * stuff and returns a #firefly_transport_connection with everything
 * set.
 *
 * To recieve data on this connection, a read operation should be performed on
 * the provided #firefly_transport_llp.
 *
 * @param llp The #firefly_transport_llp the connection is being opened
 * on.
 * @param remote_ip_addr The IP address to connect to.
 * @param remote_port The port to connect to.
 * @return The transport specific stuff of the connection as a
 * #firefly_transport_connection.
 * @retval NULL Returns \c NULL upon failure.
 */
struct firefly_transport_connection *firefly_transport_connection_udp_lwip_new(
		struct firefly_transport_llp *llp,
		char *remote_ip_addr,
		unsigned short remote_port);

#endif
