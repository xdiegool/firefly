#ifndef TEST_TRANSPORT_H
#define TEST_TRANSPORT_H

void protocol_data_received_repl(struct firefly_connection *conn,
								 unsigned char *data,
								 size_t size);

#endif
