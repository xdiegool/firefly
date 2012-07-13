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
 * @struct transport_llp_udp_posix
 * @brief An opaque UDP specific link layer port data.
 */
extern struct transport_llp_udp_posix;

/**
 * @struct protocol_connection_udp_posix
 * @brief An opaque UDP specific connection related data.
 */
extern struct protocol_connection_udp_posix;

/**
 * @brief Allocates and initializes a new \c transport_llp with UDP specific
 * data and open an UDP socket bound to the specified \a local_port.
 *
 * @param local_port The port to bind the new socket to.
 * @param on_conn_recv The callback to call when a new connection is received
 * @return A pointer to the created \c transport_llp.
 */
struct transport_llp *transport_llp_udp_posix_new(unsigned short local_port,
	       	application_on_conn_recv_cb on_conn_recv);

/**
 * @brief Close the socket and free any resources associated with this
 * transport_llp.
 *
 * The resources freed include all connections and resources freed due to
 * freeing a connection.
 *
 * @param llp The transport_llp to free.
 */
void transport_llp_udp_posix_free(struct transport_llp **llp);

/**
 * @brief Opens a connection on the provided link layer port.
 *
 * To recieve data on this connection, a read operation should be performed on
 * the provided transport_llp.
 *
 * @param ip_addr The IP address to connect to.
 * @param port The port to connect to.
 * @param llp The \c #transport_llp to open a connection on.
 * @return The newly opened connection.
 * @retval NULL Returns \c NULL upon failure.
 */
struct firefly_connection *transport_connection_udp_posix_open(char *ip_addr,
		unsigned short port, struct transport_llp *llp);

/**
 * @brief Free the connection and any resources associated with it.
 *
 * The freed resources includes all channels.
 *
 * @param conn A pointer to the pointer to the #firefly_connection to free. The pointer
 * will be set to \c NULL.
 */
void transport_connection_udp_posix_free(struct firefly_connection **conn);

/**
 * @brief Read data from the connection and fire events.
 *
 * This function is the backbone of the entire API. It can be viewed as an event
 * dispatcher. Any events will be fired from here and all data received will be
 * distributed to the different layers of the system from here. This function
 * is \b blocking.
 *
 * If data is received from an unknown source a new connection will be created
 * and the \c application_on_conn_recv_cb() associated with this transport_llp
 * will called with the new connection as argument. See
 * application_on_conn_recv_cb() for more information regarding received
 * connections.
 *
 * @param llp The Link Layer Port to read data from.
 */
void transport_llp_udp_posix_read(struct transport_llp *llp);

/**
 * @brief Write data on the specified connection.
 *
 * @param data The data to be written.
 * @param data_size The size of the data to be written.
 * @param conn The connection to written the data on.
 */
void transport_write_udp_posix(unsigned char *data, size_t data_size,
		struct firefly_connection *conn);

#endif
