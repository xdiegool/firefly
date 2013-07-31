/**
 * @file
 * @brief UDP specific and private transport structures and functions.
 */

#ifndef FIREFLY_TRANSPORT_UDP_LWIP_PRIVATE_H
#define FIREFLY_TRANSPORT_UDP_LWIP_PRIVATE_H

#include <transport/firefly_transport.h>
#include <signal.h>
#include <lwip/udp.h>

#include <utils/firefly_event_queue.h>

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
	u16_t local_port;	/**< The local port to listen on. */
	firefly_on_conn_recv_udp_lwip_f on_conn_recv; /**< The callback to be called
							when a new connection is
							detected. */
	struct firefly_event_queue *event_queue;
};

/**
 * @brief UDP specific connection related data.
 */
struct firefly_transport_connection_udp_lwip {
	struct udp_pcb *upcb;	/**< UDP process control block. Share between
				  all connections. Don't dealloc this since the
				  owner is the llp_udp this connection is in. */
	struct ip_addr *remote_ip_addr; /**< The address to the remote node of
						this connection */
	u16_t remote_port; /**< The source port of this connection. */
	struct firefly_transport_llp *llp;
};

/**
 * @brief The argument type of a read event.
 */
struct firefly_event_llp_read_udp_lwip {
	struct firefly_transport_llp *llp;	/**< The LLP to read on. */
	struct ip_addr *ip_addr; /**< The LLP to read on. */
	struct pbuf *p; /**< The pbuf containing the payload read. */
	u16_t port; /**< The port to read from which the data was read. */
};

/**
 * Mark the packet with the provided id as acknowledged. The packet is
 * removed from the resend_queue.
 *
 * @note Currently not implemented.
 *
 * @param pkt_id The ID of the packet that has been acknowledged and
 * should be removed.
 * @param conn The conn the packet was sent on.
 */
void firefly_transport_udp_lwip_ack(unsigned char pkt_id,
		struct firefly_connection *conn);

/**
 * @brief The event that handles parsing data and getting it further up
 * in the protocol chain. Called by the event constructed by
 * #udp_lwip_recv_callback.
 *
 * @param event_arg See #firefly_event_llp_read_udp_lwip.
 * @see #udp_lwip_recv_callback()
 */
int firefly_transport_udp_lwip_read_event(void *event_arg);

/**
 * @brief The event that handles freeing the LWIP LLP.
 *
 * Called by the event constructed by
 * #firefly_transport_llp_udp_lwip_free.
 *
 * @param event_arg The argument for this event.
 * @return An int indicating success (0) or failure (not 0).
 */
int firefly_transport_llp_udp_lwip_free_event(void *event_arg);

/**
 * @brief Internal function to compare IP addresses, one in the context
 * parameter and the remote one in the #firefly_connection.
 *
 * @param conn The connection to compare the remote address of.
 * @param context The IP address to compare the connection's with.
 *
 * @return A boolean indicating whether they are equal or not.
 * @retval false If the addresses are not equal.
 * @retval true If the addresses are equal.
 */
bool transport_udp_lwip_conn_eq_ipaddr(struct firefly_connection *conn,
		void *context);

/**
 * @brief Converts a string representation of an IPv4 address of the format
 * "a.b.c.d" to an LWIP representation of that address.
 *
 * @param ip_str The IP address string in dot base 255 format.
 * @return An LWIP representation of the address.
 * @retval NULL On failure.
 */
struct ip_addr *str_to_ip_addr(const char *ip_str);

/**
 * @brief Converts a LWIP represented IPv4 address to a string of the format
 * "a.b.c.d".
 *
 * @param ip_addr The IP address struct that will be converted.
 *
 * @return The IP address string in a dot base 255 format.
 * @retval NULL On failure.
 */
char *ip_addr_to_str(struct ip_addr *ip_addr);

#endif
