/**
 * @file
 * @brief The public API of the transport UDP LWIP with specific structures and
 * functions.
 */
#ifndef FIREFLY_TRANSPORT_UDP_LWIP_H
#define FIREFLY_TRANSPORT_UDP_LWIP_H

#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport.h>

#include "eventqueue/firefly_event_queue.h"

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
// TODO rename to _f to follow our function typedef standard.
typedef struct firefly_connection *(*firefly_on_conn_recv_udp_lwip)(
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
		char *local_ip_addr,
		unsigned short local_port,
		firefly_on_conn_recv_udp_lwip on_conn_recv);

/**
 * @brief Close the socket and free any resources associated with this
 * firefly_transport_llp.
 *
 * The resources freed include all connections and resources freed due to
 * freeing a connection.
 *
 * @param llp The firefly_transport_llp to free.
 */
void firefly_transport_llp_udp_lwip_free(struct firefly_transport_llp **llp);

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
struct firefly_connection *firefly_transport_connection_udp_lwip_open(
		firefly_channel_is_open_f on_channel_opened,
		firefly_channel_closed_f on_channel_closed,
		firefly_channel_accept_f on_channel_recv,
		struct firefly_event_queue *event_queue,
		char *ip_addr,
		unsigned short port,
		struct firefly_transport_llp *llp);

/**
 * @brief Set the state of the connection to closed.
 *
 * @param conn The connection to set the sate on.
 */
void firefly_transport_connection_udp_lwip_close(
		struct firefly_connection *conn);

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
		struct firefly_connection **conn);

/**
 * @brief Free any closed conenctions on the given firefly_transport_llp.
 *
 * @param llp The llp to search for closed connections on.
 * @return The number of connections closed.
 * @retval Negative number on error.
 * @retval 0 if no connection was closed.
 */
// TODO should this really be a public API function??
int firefly_transport_udp_lwip_clean_up(struct firefly_transport_llp *llp);

/**
 * @brief Write data on the specified connection.
 *
 * @param data The data to be written.
 * @param data_size The size of the data to be written.
 * @param conn The connection to written the data on.
 */
// TODO should this really be a public API function??
void firefly_transport_udp_lwip_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn);
#endif