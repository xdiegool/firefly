#ifndef FIREFLY_TRANSPORT_UDP_POSIX_H
#define FIREFLY_TRANSPORT_UDP_POSIX_H

#include <stdbool.h>
#include <netinet/in.h>

#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport.h>

struct transport_llp_udp_posix {
	int local_udp_socket;
	struct sockaddr_in *local_addr;
};

struct protocol_connection_udp_posix {
	struct sockaddr_in *remote_addr;
};

struct transport_llp *transport_llp_udp_posix_new(unsigned short local_port,
	       	application_on_conn_recv_cb);

void transport_llp_udp_posix_read(struct transport_llp *llp);


#endif
