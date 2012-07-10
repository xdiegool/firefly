#ifndef FIREFLY_TRANSPORT_UDP_POSIX_PRIVATE_H
#define FIREFLY_TRANSPORT_UDP_POSIX_PRIVATE_H

/**
 * @brief Find the connection with the specified address.
 * Find the connection with the specified address ascosiated with
 * the supplied llp. The connection with a matching address is returned,
 * if none is found NULL is returned.
 * @param addr The address of the connection to find.
 * @return The connection with a matching address.
 * @retval NULL Is returned if no connection with a matching address was found.
 */
struct connection *find_connection_by_addr(struct sockaddr_in *addr,
		struct transport_llp *llp);

#endif
