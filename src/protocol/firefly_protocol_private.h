#ifndef FIREFLY_PROTOCOL_PRIVATE_H
#define FIREFLY_PROTOCOL_PRIVATE_H

/**
 * @brief The function called by the transport layer upon received data.
 * @param conn The connection the data is ascosiated with.
 * @param data The received data.
 * @param size The size of the received data.
 */
void protocol_data_received(struct connection *conn,
	       	unsigned char *data, size_t size);

#endif
