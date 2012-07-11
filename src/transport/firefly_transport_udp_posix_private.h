/**
 * @file
 * @brief UDP specific and private transport structures and functions.
 */

#ifndef FIREFLY_TRANSPORT_UDP_POSIX_PRIVATE_H
#define FIREFLY_TRANSPORT_UDP_POSIX_PRIVATE_H

/**
 * @brief Finds the \c struct #connection with the specified address.
 *
 * Find the connection with the specified address ascosiated with
 * the supplied \c llp. The connection with a matching address
 * is returned, if none is found \c NULL is returned.
 *
 * @param addr The address of the connection to find.
 * @param llp The link layer port to search for the connection in.
 * @return The connection with a matching address.
 * @retval NULL is returned if no connection with a matching address was found.
 * @retval struct #connection* is returned with the matching address if it was
 * found.
 */
struct connection *find_connection_by_addr(struct sockaddr_in *addr,
		struct transport_llp *llp);

/**
 * @brief Compares two \c struct sockaddr_in.
 *
 * @param one The first element.
 * @param other The second element to compare against the first.
 * @return \c true if the two parameters are equal and \c false otherwise.
 */
bool sockaddr_in_eq(struct sockaddr_in *one, struct sockaddr_in *other);

/**
 * @brief Adds a connection to the connection list in \c llp.
 *
 * @param conn The connection to add.
 * @param llp The link layer port structure to add the connection to.
 */
void add_connection_to_llp(struct connection *conn, struct transport_llp *llp);
#endif
