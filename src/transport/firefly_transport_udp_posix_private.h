/**
 * @file
 * @brief UDP specific and private transport structures and functions.
 */

#ifndef FIREFLY_TRANSPORT_UDP_POSIX_PRIVATE_H
#define FIREFLY_TRANSPORT_UDP_POSIX_PRIVATE_H

#include <transport/firefly_transport.h>


/**
 * @brief UDP specific link layer port data.
 */
struct transport_llp_udp_posix {
	int local_udp_socket; /**< The file descriptor of the UDP socket */
	struct sockaddr_in *local_addr; /**< The address the socket is bound to */
	/* buffer related stuff */
	size_t scale_back_nbr;	/**< The number of pkgs before scaleback */
	size_t nbr_smaller; 	/**< The number of smaller pkgs received */
	size_t scale_back_size;	/**< The next largest pkg seen among the n last pkgs */
	size_t recv_buf_size;	/**< Current buffer size */
	unsigned char *recv_buf;	/**< Pointer t the recv buffer  */
};

/**
 * @brief UDP specific connection related data.
 */
struct protocol_connection_udp_posix {
	struct sockaddr_in *remote_addr; /**< The address to the remote node of
						this connection */
	int socket; /**< The socket file descriptor associated with this
				connection. */
};

/**
 * @brief Finds the \c struct #connection with the specified address.
 *
 * Find the connection with the specified address associated with
 * the supplied \a llp. The connection with a matching address
 * is returned, if none is found \c NULL is returned.
 *
 * @param addr The address of the connection to find.
 * @param llp The link layer port to search for the connection in.
 * @return The  #firefly_connection with a matching address.
 * @retval NULL is returned if no connection with a matching address was found.
 * @retval struct #connection* is returned with the matching address if it was
 * found.
 */
struct firefly_connection *find_connection_by_addr(struct sockaddr_in *addr,
		struct firefly_transport_llp *llp);

/**
 * @brief Compares two \c struct sockaddr_in.
 *
 * @param one The first element.
 * @param other The second element to compare against the first.
 * @return \c true if the two parameters are equal and \c false otherwise.
 */
bool sockaddr_in_eq(struct sockaddr_in *one, struct sockaddr_in *other);

/**
 * @brief Adds a connection to the connection list in \a llp.
 *
 * @param conn The connection to add.
 * @param llp The link layer port structure to add the connection to.
 */
void add_connection_to_llp(struct firefly_connection *conn,
		struct firefly_transport_llp *llp);

#endif
