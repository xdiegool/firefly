/**
 * @file
 * @brief UDP specific and private transport structures and functions.
 */

#ifndef FIREFLY_TRANSPORT_UDP_LWIP_PRIVATE_H
#define FIREFLY_TRANSPORT_UDP_LWIP_PRIVATE_H

#include <transport/firefly_transport.h>
#include <signal.h>

#define FIREFLY_CONNECTION_OPEN (1)
#define FIREFLY_CONNECTION_CLOSED (0)

/**
 * @brief UDP specific link layer port data.
 */
struct transport_llp_udp_lwip {
	struct udp_pcb *upcb;	/**< UDP process control block. */
	struct ip_addr local_ip_addr; /**< The local IP address to bind to. Can
					be IP_ADDR_ANY to bind to all known
					interfaces. */
	firefly_on_conn_recv_udp_lwip on_conn_recv; /**< The callback to be called
							when a new connection is
							detected. */
};

/**
 * @brief UDP specific connection related data.
 */
struct protocol_connection_udp_lwip {
	struct ip_addr *remote_addr;; /**< The address to the remote node of
						this connection */
	sig_atomic_t open;	/**< Flags the state of the connection. */
};

/**
 * @brief Finds the \c struct #firefly_connection with the specified address.
 *
 * Find the connection with the specified address associated with
 * the supplied \a llp. The connection with a matching address
 * is returned, if none is found \c NULL is returned.
 *
 * @param ip_addr The address of the connection to find.
 * @param llp The link layer port to search for the #firefly_connection in.
 * @return The #firefly_connection with a matching address.
 * @retval NULL is returned if no connection with a matching address was found.
 * @retval struct #firefly_connection* is returned with the matching address if
 * it was found.
 */
struct firefly_connection *find_connection_by_addr(struct ip_addr *addr,
		struct firefly_transport_llp *llp);


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
 * @param remote_addr The address of the remote node.
 *
 * @return A new firefly_connection with udp posix specific data.
 * @retval NULL on error.
 */
struct firefly_connection *firefly_connection_udp_posix_new(
					firefly_channel_is_open_f on_channel_opened,
					firefly_channel_closed_f on_channel_closed,
					firefly_channel_accept_f on_channel_recv,
					struct firefly_event_queue *event_queue,
					struct firefly_transport_llp *llp,
					struct sockaddr_in *remote_addr);

#endif
