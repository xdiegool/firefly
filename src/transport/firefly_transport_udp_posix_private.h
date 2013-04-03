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
	firefly_on_conn_recv_pudp on_conn_recv; /**< The callback to be called
							when a new connection is
							detected. */
	struct firefly_event_queue *event_queue;
	struct resend_queue *resend_queue;
};

/**
 * @brief UDP specific connection related data.
 */
struct protocol_connection_udp_posix {
	struct sockaddr_in *remote_addr; /**< The address to the remote node of
						this connection */
	int socket; /**< The socket file descriptor associated with this
				connection. */
	struct firefly_transport_llp *llp;
	unsigned int timeout; /**< The time between resends on this connection. */
};

struct firefly_event_llp_read_udp_posix {
	struct firefly_transport_llp *llp;
	struct sockaddr_in *addr;
	unsigned char *data;
	size_t len;
};

int firefly_transport_udp_posix_read_event(void *event_arg);

/**
 * @brief Compares the \c struct #firefly_connection with the specified address.
 *
 * @param conn The \c struct #firefly_connection to compare the address of.
 * @param context The address of the connection to find.
 * @retval true if the address supplied matches the addrss of
 * the \c struct #firefly_connection.
 */
bool connection_eq_inaddr(struct firefly_connection *conn, void *context);


/**
 * @brief Compares two \c struct sockaddr_in.
 *
 * @param one The first element.
 * @param other The second element to compare against the first.
 * @retval true if the two parameters are equal
 * @retval false otherwise.
 */
bool sockaddr_in_eq(struct sockaddr_in *one, struct sockaddr_in *other);

/**
 * @brief Convert a struct sockaddr_in to a string representation.
 * 
 * @param addr The struct sockaddr_in to convert.
 * @param ip_addr Where the resulting string should be written.
 * @warning The string ip_addr must be at least of length INET_ADDRSTRLEN.
 */
void sockaddr_in_ipaddr(struct sockaddr_in *addr, char *ip_addr);

/**
 * @brief Get the port in an struct sockaddr_in.
 * 
 * @param addr The struct sockaddr_in
 * @retnr The port.
 */
unsigned short sockaddr_in_port(struct sockaddr_in *addr);

int firefly_transport_llp_udp_posix_free_event(void *event_arg);

/**
 * @brief Allocates and initializes a new connection with udp posix specific
 * data.
 *
 * @param on_channel_opened Callback for when a channel has been opened.
 * @param on_channel_closed Callback for when a channel has been closed.
 * @param on_channel_recv Callback for when a channel has been recveived.
 * @param event_queue The event queue all events relating to this connection is
 * offered to.
 * @param llp The link layer port this connection receives data from.
 * @param timeout The time in ms between resends.
 * @param remote_addr The address of the remote node.
 *
 * @return A new firefly_connection with udp posix specific data.
 * @retval NULL on error.
 */
struct firefly_connection *firefly_connection_udp_posix_new(
					firefly_channel_is_open_f on_channel_opened,
					firefly_channel_closed_f on_channel_closed,
					firefly_channel_accept_f on_channel_recv,
					struct firefly_transport_llp *llp,
					unsigned int timeout,
					struct sockaddr_in *remote_addr);

/**
 * @brief Free the connection and any resources associated with it.
 *
 * The freed resources includes all channels.
 *
 * @param conn A pointer to the pointer to the #firefly_connection to free. The
 * pointer will be set to \c NULL.
 */
void firefly_transport_connection_udp_posix_free(
		struct firefly_connection *conn);

void firefly_transport_udp_posix_ack(unsigned char pkt_id,
		struct firefly_connection *conn);

#endif
