/**
 * @file
 * @brief UDP specific and private transport structures and functions.
 */

#ifndef FIREFLY_TRANSPORT_UDP_POSIX_PRIVATE_H
#define FIREFLY_TRANSPORT_UDP_POSIX_PRIVATE_H

#include <transport/firefly_transport.h>
#include <signal.h>

#include <utils/firefly_event_queue.h>
#include <utils/firefly_resend_posix.h>

#include "transport/firefly_transport_private.h"

/**
 * @brief UDP specific link layer port data.
 */
struct transport_llp_udp_posix {
	int local_udp_socket; /**< The file descriptor of the UDP socket */
	struct sockaddr_in *local_addr; /**< The address the socket is bound to. */
	firefly_on_conn_recv_pudp on_conn_recv; /**< The callback to be called when
											  a new connection is received. */
	struct firefly_event_queue *event_queue; /**< The event queue to push new
											   events on. */
	struct resend_queue *resend_queue; /**< The resend queue managing important
										 packets. */
};

/**
 * @brief UDP specific connection related data.
 */
struct firefly_transport_connection_udp_posix {
	struct sockaddr_in *remote_addr; /**< The address of the remote node of this
									   connection */
	int socket; /**< The socket file descriptor associated with this
				  connection. */
	struct firefly_transport_llp *llp; /**< The \a llp this connection is
										 associated with. */
	unsigned int timeout; /**< The time between resends on this connection. */
};

/**
 * @brief The argument type of a read event.
 */
struct firefly_event_llp_read_udp_posix {
	struct firefly_transport_llp *llp; /**< The llp the data was read on. */
	struct sockaddr_in addr; /**< The address the data was sent from. */
	size_t len; /**< The number of bytes in the data field. */
	unsigned char data[]; /**< The read data. */
};

/**
 * @brief The event handling any read data.
 *
 * @param event_arg See #firefly_event_llp_read_udp_posix.
 * @see #firefly_transport_udp_posix_read()
 */
int firefly_transport_udp_posix_read_event(void *event_arg);

/**
 * @brief The event handling llp free.
 *
 * @param event_arg The #firefly_transport_llp to free.
 * @see #firefly_transport_llp_udp_posix_free().
 */
int firefly_transport_llp_udp_posix_free_event(void *event_arg);

/**
 * @brief Compares the \c struct #firefly_connection with the specified address.
 *
 * @param conn The \c struct #firefly_connection to compare the address of.
 * @param context The address of the connection to find, must be of type
 * \c struct \c sockaddr_in.
 * @retval true if the address supplied matches the addrss of
 * the \c struct #firefly_connection.
 * @retval false otherwise
 */
bool connection_eq_inaddr(struct firefly_connection *conn, void *context);


/**
 * @brief Compares two \c struct \c sockaddr_in.
 *
 * @param one The first element.
 * @param other The second element to compare against the first.
 * @retval true if the two addresses are equal with respect to ip address and
 * port number.
 * @retval false otherwise.
 */
bool sockaddr_in_eq(struct sockaddr_in *one, struct sockaddr_in *other);

/**
 * @brief Get the ip address of a \c struct \c sockaddr_in as a string.
 * 
 * @param addr The \c struct \c sockaddr_in.
 * @param ip_addr The preallocated memory to save the string in.
 * @warning The string ip_addr must be at least of length \c INET_ADDRSTRLEN.
 */
void sockaddr_in_ipaddr(struct sockaddr_in *addr, char *ip_addr);

/**
 * @brief Get the port number in an struct sockaddr_in.
 * 
 * @param addr The \c struct \c sockaddr_in
 * @return The port number.
 */
unsigned short sockaddr_in_port(struct sockaddr_in *addr);

/**
 * @brief Write data on the specified connection. Implements
 * #firefly_transport_connection_write_f.
 *
 * @param data The data to be written.
 * @param data_size The size of the data to be written.
 * @param conn The connection to written the data on.
 * @param important If true the packet will be resent until it is acked by
 * calling #firefly_transport_udp_posix_ack or max retries is reached.
 * @param id The variable to save the resend packed id in.
 * @see #firefly_transport_connection_write_f()
 */
void firefly_transport_udp_posix_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id);

/**
 * @brief Ack an important packed. Removes the packet from the resend queue.
 * Implements #firefly_transport_connection_ack_f()
 *
 * @param pkt_id The id previously set by #firefly_transport_udp_posix_write.
 * @param conn The connection the packet was sent on.
 * @see #firefly_transport_connection_ack_f()
 */
void firefly_transport_udp_posix_ack(unsigned char pkt_id,
		struct firefly_connection *conn);

#endif
