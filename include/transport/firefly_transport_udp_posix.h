/**
 * @file
 * @brief The public API of the transport UDP POSIX with specific structures and
 * functions.
 */
#ifndef FIREFLY_TRANSPORT_UDP_POSIX_H
#define FIREFLY_TRANSPORT_UDP_POSIX_H

#include <pthread.h>
#include <stdbool.h>
#include <netinet/in.h>

#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport.h>
#include <utils/firefly_event_queue.h>

/**
 * @brief The default interval between resending important packets.
 */
#define FIREFLY_TRANSPORT_UDP_POSIX_DEFAULT_TIMEOUT (500)
/**
 * @brief The default number of retries to send an important packet before
 * giving up.
 */
#define FIREFLY_TRANSPORT_UDP_POSIX_DEFAULT_RETRIES (5)

/**
 * @brief An opaque UDP specific link layer port data.
 */
struct firefly_transport_llp_udp_posix;

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
 * @param ip_addr The IP addr of the remote node.
 * @param port The port number of the remote node.
 * @return Event id or 0.
 * @retval >0 A new connection was opened and the read data will propagate as
 * soon as the connection is completely open.
 * @retval 0 The new connection was refused and the read data is discarded.
 */
typedef int64_t (*firefly_on_conn_recv_pudp)(
		struct firefly_transport_llp *llp,
		const char *ip_addr, unsigned short port);

/**
 * @brief Allocates and initializes a new \c #firefly_transport_llp with UDP
 * specific data and open an UDP socket bound to the specified \a local_port.
 *
 * @param local_port The port to bind the new socket to.
 * @param on_conn_recv The callback to call when a new connection is received.
 * If it is NULL no received connections will be accepted.
 * @param event_queue The event queue to push spawned events to.
 * @return A pointer to the created \c firefly_transport_llp.
 * @retval NULL on error.
 */
struct firefly_transport_llp *firefly_transport_llp_udp_posix_new(
		unsigned short local_port,
		firefly_on_conn_recv_pudp on_conn_recv,
		struct firefly_event_queue *event_queue);

/**
 * @brief Through events, close the socket and free any resources associated
 * with this firefly_transport_llp.
 *
 * The resources freed include all connections and resources freed due to
 * freeing a connection.
 *
 * @param llp The firefly_transport_llp to free.
 */
void firefly_transport_llp_udp_posix_free(struct firefly_transport_llp *llp);

/**
 * @brief Allocates and initializes the transport layer specific data of a
 * #firefly_connection. It shall be supplied as parameter to
 * #firefly_connection_open().
 *
 * @param llp The \c #firefly_transport_llp to associate the data with.
 * @param remote_ipaddr The IP address to connect to.
 * @param remote_port The port to connect to.
 * @param timeout The time in ms between resends.
 * @return The transport specific data ready to be supplied as argument to
 * #firefly_connection_open().
 * @retval NULL upon failure.
 * @see #firefly_connection_open()
 */
struct firefly_transport_connection *firefly_transport_connection_udp_posix_new(
		struct firefly_transport_llp *llp,
		const char *remote_ipaddr,
		unsigned short remote_port,
		unsigned int timeout);

/**
 * @brief Start reader and resend thread. Both will run until stopped with
 * firefly_transport_udp_posix_stop(). If either pthread_t argument is NULL the
 * corresponding thread will not be started.
 *
 * @param llp The LLP to run.
 * @param reader The handle to the reader thread. If NULL the reader thread will
 * not be started.
 * @param resend The handle to the resend thread. If NULL the resend thread will
 * not be started.
 * @return Integer indicating success or failure.
 * @retval 0 if successfull.
 * @retval <0 upon error.
 */
int firefly_transport_udp_posix_run(struct firefly_transport_llp *llp,
		pthread_t *reader, pthread_t *resend);

/**
 * @brief Stop reader and resend thread. Any thread to be stopped must have been
 * started with firefly_transport_udp_posix_run(), if not the result is
 * undefined. If either pthread_t argument is NULL the corresponding thread
 * will not be stopped.
 *
 * @param llp The LLP to stop.
 * @param reader The handle to the reader thread. If NULL the reader thread will
 * not be stopped.
 * @param resend The handle to the resend thread. If NULL the resend thread will
 * not be stopped.
 * @return Integer indicating success or failure.
 * @retval 0 if successfull.
 * @retval <0 upon error.
 */
int firefly_transport_udp_posix_stop(struct firefly_transport_llp *llp,
		pthread_t *reader, pthread_t *resend);

/**
 * @brief Read data from the #firefly_transport_llp. Any read data will be
 * included in an event pushed to the #firefly_event_queue.
 *
 * The read data will be distributed to the connection opened to the remote
 * address the data is sent from.
 *
 * If no such connection exists the #firefly_on_conn_recv_pudp will be called,
 * if it is NULL the data will be discarded.
 *
 * This function is blocking.
 *
 * @param llp The Link Layer Port to read data from.
 * @see firefly_on_conn_recv_pudp
 */
void firefly_transport_udp_posix_read(struct firefly_transport_llp *llp);

#endif
