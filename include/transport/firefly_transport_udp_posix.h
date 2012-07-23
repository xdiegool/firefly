/**
 * @file
 * @brief The public API of the transport UDP POSIX with specific structures and
 * functions.
 */
#ifndef FIREFLY_TRANSPORT_UDP_POSIX_H
#define FIREFLY_TRANSPORT_UDP_POSIX_H

#include <stdbool.h>
#include <netinet/in.h>

#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport.h>

/**
 * @brief An opaque UDP specific link layer port data.
 */
struct firefly_transport_llp_udp_posix;

/**
 * @brief Allocates and initializes a new \c transport_llp with UDP specific
 * data and open an UDP socket bound to the specified \a local_port.
 *
 * @param local_port The port to bind the new socket to.
 * @param on_conn_recv The callback to call when a new connection is received
 * @return A pointer to the created \c firefly_transport_llp.
 */
struct firefly_transport_llp *firefly_transport_llp_udp_posix_new(
		unsigned short local_port, firefly_on_conn_recv on_conn_recv);

/**
 * @brief Close the socket and free any resources associated with this
 * firefly_transport_llp.
 *
 * The resources freed include all connections and resources freed due to
 * freeing a connection.
 *
 * @param llp The firefly_transport_llp to free.
 */
void firefly_transport_udp_posix_free(struct firefly_transport_llp **llp);

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
struct firefly_connection *firefly_transport_connection_udp_posix_open(
		char *ip_addr, unsigned short port,
	       	struct firefly_transport_llp *llp);

/**
 * @brief Free the connection and any resources associated with it.
 *
 * The freed resources includes all channels.
 *
 * @param conn A pointer to the pointer to the #firefly_connection to free. The
 * pointer will be set to \c NULL.
 */
void firefly_transport_connection_udp_posix_free(
		struct firefly_connection **conn);

/**
 * @brief Read data from the connection and fire events.
 *
 * This function is the backbone of the entire API. It can be viewed as an event
 * dispatcher. Any events will be fired from here and all data received will be
 * distributed to the different layers of the system from here. This function
 * is \b blocking.
 *
 * If data is received from an unknown source a new connection will be created
 * and the \c application_on_conn_recv_cb() associated with this
 * firefly_transport_llp * will called with the new connection as argument. See
 * application_on_conn_recv_cb() for more information regarding received
 * connections.
 *
 * @param llp The Link Layer Port to read data from.
 */
void firefly_transport_udp_posix_read(struct firefly_transport_llp *llp);

/**
 * @brief Write data on the specified connection.
 *
 * @param data The data to be written.
 * @param data_size The size of the data to be written.
 * @param conn The connection to written the data on.
 */
void firefly_transport_write_udp_posix(unsigned char *data, size_t data_size,
		struct firefly_connection *conn);

/**
 * @brief Set the number of strictly smaller packages required before the size
 *        of the receive buffer is decreased.
 *
 * @param llp The llp to apply the change on.
 * @param nbr The number of package to set as the new threshold. \b Must be greater
 *        than zero.
 */
void firefly_transport_udp_posix_set_n_scaleback(struct firefly_transport_llp *llp,
												 unsigned int nbr);

#endif
