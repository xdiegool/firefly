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
 * @brief An opaque UDP specific link layer port data.
 */
struct firefly_transport_llp_udp_lwip;

/**
 * @brief This callback will be called when a new connection is received.
 *
 * This function is implemented by the application layer. It will be called
 * when the transport layer receives a new connection. It will be called with
 * address of the remote node of the connection as argument. The application
 * layer may open a new connection to this remote node by calling
 * firefly_transport_connection_udp_lwip_open() with the address as argument.
 * If the connection is opened it must be returned. If no new connection is
 * opened return NULL.
 *
 * @param ip_addr The IP addr of the remote node.
 * @param port The port number of the remote node.
 * @return The opened connection.
 * @retval NULL if no connection was opened.
 */
typedef bool (*firefly_on_conn_recv_udp_lwip_f)(
		struct firefly_transport_llp *llp, char *ip_addr,
		unsigned short port);

/**
 * @brief Allocates and initializes a new \c transport_llp with UDP specific
 * data and open an UDP socket bound to the specified \a local_port.
 *
 * @param local_ip_addr The local IP address to bind to. Can be IP_ADDR_ANY to
 * bind to all known interfaces.
 * @param local_port The port to bind the new socket to.
 * @param on_conn_recv The callback to call when a new connection is received
 * @return A pointer to the created \c firefly_transport_llp.
 */
struct firefly_transport_llp *firefly_transport_llp_udp_lwip_new(
		char *local_ip_addr, unsigned short local_port,
		firefly_on_conn_recv_udp_lwip_f on_conn_recv,
		struct firefly_event_queue *event_queue);

/**
 * @brief Close the socket and free any resources associated with this
 * firefly_transport_llp.
 *
 * The resources freed include all connections and resources freed due to
 * freeing a connection.
 *
 * @param llp The firefly_transport_llp to free.
 */
void firefly_transport_llp_udp_lwip_free(struct firefly_transport_llp *llp);

/**
 * @brief Opens a connection on the provided link layer port.
 *
 * To recieve data on this connection, a read operation should be performed on
 * the provided #firefly_transport_llp.
 *
 * @param ip_addr The IP address to connect to.
 * @param port The port to connect to.
 * @param llp The \c #firefly_transport_llp to open a connection on.
 * @return The newly opened connection.
 * @retval NULL Returns \c NULL upon failure.
 */
struct firefly_transport_connection *firefly_transport_connection_udp_lwip_new(
		struct firefly_transport_llp *llp,
		char *remote_ip_addr,
		unsigned short remote_port);

/**
 * @brief Free the connection and any resources associated with it.
 *
 * The freed resources includes all channels.
 *
 * @param conn A pointer to the pointer to the #firefly_connection to free. The
 * pointer will be set to \c NULL.
 */
// TODO should this really be a public API function??
void firefly_transport_connection_udp_lwip_free(
		struct firefly_connection *conn);

/**
 * @brief Write data on the specified connection.
 *
 * @param data The data to be written.
 * @param data_size The size of the data to be written.
 * @param conn The connection to written the data on.
 * @param important If true the packet will be resent unitl acked.
 * @param id If important is true the id is a return value and will contain the
 * identifier of the packet which must be used when acking the packet.
 */
// TODO should this really be a public API function??
void firefly_transport_udp_lwip_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id);
#endif
