#ifndef FIREFLY_TRANSPORT_UDP_POSIX_PRIVATE_H
#define FIREFLY_TRANSPORT_UDP_POSIX_PRIVATE_H

/**
 * find_connection_by_addr
 * @param	addr
 * @param	llp
 * @return	struct connection *
 */
struct connection *find_connection_by_addr(struct sockaddr_in *addr,
		struct transport_llp *llp);

#endif
