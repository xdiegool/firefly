#ifndef FIREFLY_TRANSPORT_ETH_XENO_H
#define FIREFLY_TRANSPORT_ETH_XENO_H

#include <transport/firefly_transport.h>
#include <sys/time.h>

/**
 * @brief An opaque ethernet xenomai specific link layer port data.
 */
struct transport_llp_eth_xeno;

struct firefly_transport_connection_eth_xeno;

/**
 * @brief This callback will be called when a new connection is received.
 *
 * This function is implemented by the application layer. It will be called
 * when the transport layer receives a new connection. It will be called with
 * address of the remote node of the connection as argument. The application
 * layer may open a new connection to this remote node on its own.
 * If a connection is opened, the id of the event as returned by
 * #firefly_connection_open must be returned. If no new connection is
 * opened 0 must be returned.
 *
 * @param llp The \a llp the incomming connection is associated with.
 * @param mac_address The MAC addr of the remote node.
 * @return Event id or 0.
 * @retval >0 A new connection was opened and the read data will propagate as
 * soon as the connection is completely open.
 * @retval 0 The new connection was refused and the read data is discarded.
 */
typedef int64_t (*firefly_on_conn_recv_eth_xeno)(
		struct firefly_transport_llp *llp, char *mac_address);

/**
 * @brief Allocates and initializes a new \c #firefly_transport_llp with
 * ethernet xenomai specific data and open an ethernet socket bound to the
 * interface with the name \a iface_name.
 *
 * @param iface_name The name of the interface to bind the new socket to.
 * @param on_conn_recv The callback to call when a new connection is received.
 * If it is NULL no received connections will be accepted.
 * @param event_queue The event queue to push spawned events to.
 * @return A pointer to the created \c firefly_transport_llp.
 * @retval NULL on error.
 */
struct firefly_transport_llp *firefly_transport_llp_eth_xeno_new(
		const char *iface_name,
		firefly_on_conn_recv_eth_xeno on_conn_recv,
		struct firefly_event_queue *event_queue);

/**
 * @brief Through events, close the socket and free any resources associated
 * with this #firefly_transport_llp.
 *
 * The resources freed include all connections and resources freed due to
 * freeing a connection.
 *
 * @param llp The firefly_transport_llp to free.
 */
void firefly_transport_llp_eth_xeno_free(struct firefly_transport_llp *llp);

/**
 * @brief Allocates and initializes the transport layer specific data of a
 * #firefly_connection. It shall be supplied as parameter to
 * #firefly_connection_open().
 *
 * @param llp The \c #firefly_transport_llp to associate the data with.
 * @param mac_address The MAC address of the remote node.
 * @param if_name The name of the interface to send data on.
 * @return The transport specific data ready to be supplied as argument to
 * #firefly_connection_open().
 * @retval NULL upon failure.
 * @see #firefly_connection_open()
 */
struct firefly_transport_connection *firefly_transport_connection_eth_xeno_new(
		struct firefly_transport_llp *llp,
		char *mac_address,
		char *if_name);

/**
 * @brief Read data from the #firefly_transport_llp. Any read data will be
 * included in an event pushed to the #firefly_event_queue.
 *
 * The read data will be distributed to the connection opened to the remote
 * address the data is sent from.
 *
 * If no such connection exists the #firefly_on_conn_recv_eth_xeno will be
 * called, if it is NULL the data will be discarded.
 *
 * This function is blocking.
 *
 * @param llp The Link Layer Port to read data from.
 * @param timeout The time to wait for new data before aborting.
 * @see firefly_on_conn_recv_eth_xeno
 */
void firefly_transport_eth_xeno_read(struct firefly_transport_llp *llp,
		int64_t *timeout);

/**
 * @brief Get the struct with implementations of each memory function as
 * specified by #firefly_memory_funcs.
 *
 * This struct must be supplied to #firefly_connection_open() as a parameter to
 * guarantee real time operation.
 *
 * @return The xenomai implementation of #firefly_memory_funcs.
 * @see #firefly_memory_funcs
 */
struct firefly_memory_funcs *firefly_transport_eth_xeno_memfuncs();

#endif
