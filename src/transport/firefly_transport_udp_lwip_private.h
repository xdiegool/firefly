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
	firefly_on_conn_recv_udp_lwip on_conn_recv; /**< The callback to be called
							when a new connection is
							detected. */
	struct firefly_event_queue *event_queue;
};

/**
 * @brief UDP specific connection related data.
 */
struct protocol_connection_udp_lwip {
	struct udp_pcb *upcb;	/**< UDP process control block. Share between
				  all connections. Don't dealloc this since the
				  owner is the llp_udp this connection is in. */
	struct ip_addr *remote_ip_addr; /**< The address to the remote node of
						this connection */
	u16_t remote_port; /**< The source port of this connection. */
	struct firefly_transport_llp *llp;
};

struct firefly_event_llp_read_udp_posix {
	struct firefly_transport_llp *llp;
	struct ip_addr *ip_addr;
	struct pbuf *p;
	u16_t port;
};

void firefly_transport_udp_lwip_ack(unsigned char pkt_id,
		struct firefly_connection *conn);

int firefly_transport_udp_lwip_read_event(void *event_arg);

int firefly_transport_llp_udp_lwip_free_event(void *event_arg);

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
 * @param An LWIP representation of the address.
 * @return ip_str The IP address string in dot base 255 format.
 * @retval NULL On failure.
 */
char *ip_addr_to_str(struct ip_addr *ip_addr);

#endif
