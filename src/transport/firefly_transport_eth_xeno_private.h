
#ifndef FIREFLY_TRANSPORT_ETH_XENO_PRIVATE_H
#define FIREFLY_TRANSPORT_ETH_XENO_PRIVATE_H

#include <transport/firefly_transport.h>
#include "transport/firefly_transport_private.h"
#include <transport/firefly_transport_eth_xeno.h>
#include <netpacket/packet.h>	// defines sockaddr_ll
#include <signal.h>
#include <native/heap.h>
#include <native/task.h>
#include <native/mutex.h>

/**
 * @brief The protocol specified in every firefly packet. This is used to filter
 * out ethernet packets of value.
 */
#define FIREFLY_ETH_PROTOCOL	0x1337

/**
 * @brief The length of a string representation of a MAC address.
 * @see #get_mac_addr()
 */
#define MACADDR_STRLEN (18)

/**
 * @brief The ethernet xenomai specific data of a \c llp.
 */
struct transport_llp_eth_xeno {
	int socket; /**< The socket. */
	struct firefly_event_queue *event_queue; /**< The event queue to push new
											   events to. */
	firefly_on_conn_recv_eth_xeno on_conn_recv; /**< The callback to be
													 called when a new
													 connection is received. */
	struct resend_queue *resend_queue; /**< The resend queue managing important
											packets. */
	RT_TASK read_thread; /**< The handle to the thread running the read loop. */
	RT_MUTEX run_mutex;
	RT_TASK resend_thread; /**< The handle to the thread running the resend
								loop. */
	bool running; /**< Whether or not the read loop should exit. */

	RT_HEAP dyn_mem; /**< The heap implementing real time allocation of dynamic
					   memory. */
};

/**
 * @brief Ethernet posix specific connection related data.
 */
struct firefly_transport_connection_eth_xeno {
	struct sockaddr_ll *remote_addr; /**< The address of the remote node of this
									   connection */
	struct firefly_transport_llp *llp; /**< The \a llp this connection is
										 associated with. */
	int socket; /**< The socket. */
};

/**
 * @brief The event handling llp free.
 *
 * @param event_arg The #firefly_transport_llp to free.
 * @see #firefly_transport_llp_eth_xeno_free().
 */
int firefly_transport_llp_eth_xeno_free_event(void *event_arg);

/**
 * @brief Write data on the specified connection. Implements
 * #firefly_transport_connection_write_f.
 *
 * @param data The data to be written.
 * @param data_size The size of the data to be written.
 * @param conn The connection to written the data on.
 * @param important If true the packet will be resent until it is acked by
 * calling #firefly_transport_eth_xeno_ack or max retries is reached.
 * @param id The variable to save the resend packed id in.
 * @see #firefly_transport_connection_write_f()
 * @see #firefly_transport_eth_xeno_ack()
 */
void firefly_transport_eth_xeno_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id);

/**
 * @brief Ack an important packed. Removes the packet from the resend queue.
 * Implements #firefly_transport_connection_ack_f()
 *
 * @param pkt_id The id previously set by #firefly_transport_eth_xeno_write.
 * @param conn The connection the packet was sent on.
 * @see #firefly_transport_connection_ack_f()
 * @see #firefly_transport_eth_xeno_write()
 */
void firefly_transport_eth_xeno_ack(unsigned char pkt_id,
		struct firefly_connection *conn);

/**
 * @brief Get the MAC address of a \c struct \c sockaddr_ll as a string.
 *
 * @param addr The address to get the MAC address from.
 * @param mac_addr The preallocated memory to save the string in.
 * @warning mac_addr must point to at least #MACADDR_STRLEN bytes.
 */
void get_mac_addr(struct sockaddr_ll *addr, char *mac_addr);

/**
 * @brief Compares the \c struct #firefly_connection with the specified address.
 *
 * @param conn The \c struct #firefly_connection to compare the address of.
 * @param context The address of the connection to find, must be of type
 * \c struct \c sockaddr_ll.
 * @retval true if the address supplied matches the addrss of
 * the \c struct #firefly_connection.
 * @retval false otherwise
 * @see #conn_eq_f()
 */
bool connection_eq_addr(struct firefly_connection *conn, void *context);

#endif
