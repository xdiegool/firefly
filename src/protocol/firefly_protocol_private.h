#ifndef FIREFLY_PROTOCOL_PRIVATE_H
#define FIREFLY_PROTOCOL_PRIVATE_H

void protocol_data_received(struct connection *conn,
	       	unsigned char *data, size_t size);

#endif
