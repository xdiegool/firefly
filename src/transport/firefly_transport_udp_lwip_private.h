/**
 * @file
 * @brief UDP specific and private transport structures and functions.
 */

#ifndef FIREFLY_TRANSPORT_UDP_LWIP_PRIVATE_H
#define FIREFLY_TRANSPORT_UDP_LWIP_PRIVATE_H

#include <transport/firefly_transport.h>
#include <signal.h>
// TODO add lwip headers.

#define FIREFLY_CONNECTION_OPEN (1)
#define FIREFLY_CONNECTION_CLOSED (0)

/**
 * @brief UDP specific link layer port data.
 */
struct transport_llp_udp_lwip {
	struct udp_pcb *upcb;	/**< UDP process control block. */
	struct ip_addr *local_ip_addr; /**< The local IP address to bind to. Can
					be IP_ADDR_ANY to bind to all known
					interfaces. */
	u16_t local_port	/**< The local port to listen on. */
	firefly_on_conn_recv_udp_lwip on_conn_recv; /**< The callback to be called
							when a new connection is
							detected. */
};

/**
 * @brief UDP specific connection related data.
 */
struct protocol_connection_udp_lwip {
	struct *udp_pcb upcb;	/**< UDP process control block. Share between 
				  all connections. Don't dealloc this since the 
				  owner is the llp_udp this connection is in. */
	struct ip_addr *remote_ip_addr; /**< The address to the remote node of
						this connection */
	u16_t remote_port; /**< The source port of this connection. */
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
struct firefly_connection *find_connection_by_addr(struct ip_addr *ip_addr,
		struct firefly_transport_llp *llp);


/**
 * @brief Converts a string representation of an IPv4 address of the format 
 * "a.b.c.d" to an LWIP representation of that address.
 *
 * @param ip_str The IP address string in dot base 255 format.
 * @return An LWIP representation of the address.
 * @retval NULL On failure.
 */
struct ip_addr *str_to_ip_addr(char *ip_str);

/**
 * @brief Converts a LWIP represented IPv4 address to a string of the format 
 * "a.b.c.d".
 *
 * @param An LWIP representation of the address.
 * @return ip_str The IP address string in dot base 255 format.
 * @retval NULL On failure.
 */
char *ip_addr_to_str(struct *ip_addr);

#endif
