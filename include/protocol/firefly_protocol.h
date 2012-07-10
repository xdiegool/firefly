#ifndef FIREFLY_PROTOCOL_H
#define FIREFLY_PROTOCOL_H

struct connection {
	void *transport_conn_platspec; // Pointer to transport specific connection data.
};

#endif
