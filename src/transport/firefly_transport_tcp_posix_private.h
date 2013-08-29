/**
 * @file
 * @brief TCP specific and private transport structures and functions.
 */

#ifndef FIREFLY_TRANSPORT_TCP_POSIX_PRIVATE_H
#define FIREFLY_TRANSPORT_TCP_POSIX_PRIVATE_H

#include <transport/firefly_transport.h>
#include <signal.h>
#include <sys/select.h>

#include <utils/firefly_event_queue.h>
#include <utils/firefly_resend_posix.h>

#include "transport/firefly_transport_private.h"

/**
 * @brief TCP specific link layer port data.
 */
struct transport_llp_tcp_posix {
	int local_tcp_socket;                    /**< fd of the listening socket */
	int max_sock;                            /**< Max socket to select() on */
	fd_set master_set;                       /**< fd_set to select() on */
	struct sockaddr_in *local_addr;          /**< Address the socket is bound to */
	firefly_on_conn_recv_ptcp on_conn_recv;  /**< Callback when receiving new connection */
	struct firefly_event_queue *event_queue; /**< Event queue */
	pthread_t read_thread;                   /**< Thread running the read loop */
};

/**
 * @brief TCP specific connection related data.
 */
struct firefly_transport_connection_tcp_posix {
	struct sockaddr_in *remote_addr;   /**< Remote node's address for this connection */
	int socket;                        /**< Socket fd for this connection. */
	struct firefly_transport_llp *llp; /**< The llp this connection exists on. */
};

/**
 * @brief Write data on the specified connection. Implements
 * #firefly_transport_connection_write_f.
 *
 * @param data The data to be written.
 * @param data_size The size of the data to be written.
 * @param conn The connection to written the data on.
 * @param important If true the packet is considered important, unused in TCP.
 * @param id The variable to save the resend packed id in, unused in TCP.
 * @see #firefly_transport_connection_write_f()
 */
void firefly_transport_tcp_posix_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id);

#endif
