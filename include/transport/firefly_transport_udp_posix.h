/**
 * @file
 * @brief The public API of the Transport UDP Posix with specific structures and
 * functions.
 */
#ifndef FIREFLY_TRANSPORT_UDP_POSIX_H
#define FIREFLY_TRANSPORT_UDP_POSIX_H

#include <stdbool.h>
#include <netinet/in.h>

#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport.h>

/**
 * @brief UDP specific link layer port data.
 */
struct transport_llp_udp_posix {
	int local_udp_socket; /**< The file descriptor of the UDP socket */
	struct sockaddr_in *local_addr; /**< The address the socket is
						bound to */
};

/**
 * @brief UDP specific connection related data.
 */
struct protocol_connection_udp_posix {
	struct sockaddr_in *remote_addr; /**< The address to the remote node of
						this connection */
};

/**
 * @brief Allocates and initializes a new transport_llp with UDP specific data
 * and open an UDP socket bound to the specified local_port.
 *
 * @param local_port The port to bind the new socket to.
 * @param on_conn_recv The callback to call when a new connection is received
 * @return A pointer to the created transport_llp.
 */
struct transport_llp *transport_llp_udp_posix_new(unsigned short local_port,
	       	application_on_conn_recv_cb on_conn_recv);

/**
 * @brief Close the socket and free any resources associated with this
 * transport_llp.
 *
 * The resources freed include all connections and resources freed due to
 * freeing a connection
 *
 * @param llp The transport_llp to free
 */
void transport_llp_udp_posix_free(struct transport_llp **llp);

/**
 * @brief Free the connection and any resources associated with it.
 *
 * The freed resources includes all channels.
 *
 * @param conn The connection to free.
 */
void transport_connection_udp_posix_free(struct connection **conn);

/**
 * @brief Read data from the connection and fire events.
 *
 * This function is the backbone of the entire API. It can be viewed as an event
 * dispatcher. Any events will be fired from here and all data received will be
 * distributed to the different layers of the system from here. This function
 * is \b blocking.
 *
 * If data is received from an unknown source a new connection will be created
 * and the application_on_conn_recv_cb() associated with this transport_llp
 * will called with the new connection as argument. See
 * application_on_conn_recv_cb() for more information regarding received
 * connections.
 *
 * @param llp The Link Layer Port to read data from.
 */
void transport_llp_udp_posix_read(struct transport_llp *llp);

#endif
