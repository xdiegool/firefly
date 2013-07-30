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

#define FIREFLY_TRANSPORT_UDP_POSIX_DEFAULT_TIMEOUT (500)
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
 * layer may open a new connection to this remote node by calling
 * firefly_transport_connection_udp_posix_open() with the address as argument.
 * If the connection is opened it must be returned. If no new connection is
 * opened return NULL.
 *
 * @param ip_addr The IP addr of the remote node.
 * @param port The port number of the remote node.
 * @return Event id or 0.
 */
typedef int64_t (*firefly_on_conn_recv_pudp)(
		struct firefly_transport_llp *llp,
		const char *ip_addr, unsigned short port);

/**
 * @brief Allocates and initializes a new \c transport_llp with UDP specific
 * data and open an UDP socket bound to the specified \a local_port.
 *
 * @param local_port The port to bind the new socket to.
 * @param on_conn_recv The callback to call when a new connection is received.
 * If it is NULL no received connections will be accepted.
 * @return A pointer to the created \c firefly_transport_llp.
 */
struct firefly_transport_llp *firefly_transport_llp_udp_posix_new(
		unsigned short local_port,
		firefly_on_conn_recv_pudp on_conn_recv,
		struct firefly_event_queue *event_queue);

/**
 * @brief Close the socket and free any resources associated with this
 * firefly_transport_llp.
 *
 * The resources freed include all connections and resources freed due to
 * freeing a connection.
 *
 * @param llp The firefly_transport_llp to free.
 */
void firefly_transport_llp_udp_posix_free(struct firefly_transport_llp *llp);

/**
 * @brief Opens a connection on the provided link layer port.
 *
 * To recieve data on this connection, a read operation should be performed on
 * the provided #firefly_transport_llp.
 *
 * @param llp The \c #firefly_transport_llp to open a connection on.
 * @param remote_ip_addr The IP address to connect to.
 * @param remote_port The port to connect to.
 * @param timeout The time in ms between resends.
 * @param acitons The callbacks used.
 * @return The newly opened connection.
 * @retval NULL Returns \c NULL upon failure.
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
 * @return Zero or an error code.
 */
int firefly_transport_udp_posix_run(struct firefly_transport_llp *llp,
		pthread_t *reader, pthread_t *resend);

/**
 * @brief Stop reader and resend thread. Any thread to be stopped must have been
 * started with firefly_transport_udp_posix_run(), if not the result id
 * undefined. If either pthread_t argument is NULL the corresponding thread
 * will not be stopped.
 *
 * In other words, if for example the resend thread is started manually the idea
 * is to set the resend parameter to NULL and use these utilities to control the
 * reader thread.
 *
 * @param llp The LLP to stop.
 * @param reader The handle to the reader thread. If NULL the reader thread will
 * not be stopped.
 * @param resend The handle to the resend thread. If NULL the resend thread will
 * not be stopped.
 * @return Zero or an error code.
 */
int firefly_transport_udp_posix_stop(struct firefly_transport_llp *llp,
		pthread_t *reader, pthread_t *resend);

/**
 * @brief Read data from the connection and fire events.
 *
 * This function is the backbone of the entire API. It can be viewed as an event
 * dispatcher. Any events will be fired from here and all data received will be
 * distributed to the different layers of the system from here. This function
 * is \b blocking.
 *
 * If data is received from an unknown source a new connection will be created
 * and the \c application_on_conn_recv_cb() associated with this
 * firefly_transport_llp * will called with the new connection as argument. See
 * application_on_conn_recv_cb() for more information regarding received
 * connections.
 *
 * @param llp The Link Layer Port to read data from.
 */
void firefly_transport_udp_posix_read(struct firefly_transport_llp *llp);

/**
 * @brief Write data on the specified connection.
 *
 * @param data The data to be written.
 * @param data_size The size of the data to be written.
 * @param conn The connection to written the data on.
 * @param important If true the packet will be resent until it is acked by
 * calling #firefly_transport_udp_posix_ack
 */
void firefly_transport_udp_posix_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id);

#endif
