#ifndef FIREFLY_PROTOCOL_PRIVATE_H
#define FIREFLY_PROTOCOL_PRIVATE_H
#include <labcomm.h>

#include <protocol/firefly_protocol.h>

/**
 * @brief The function called by the transport layer upon received data.
 * @param conn The connection the data is ascosiated with.
 * @param data The received data.
 * @param size The size of the received data.
 */
void protocol_data_received(struct connection *conn,
	       	unsigned char *data, size_t size);

/**
 *`@brief A labcomm error to ff_error converter.
 * @param error_id The error id form the LabComm librar.
 * @param nbr_va_args Number of va_args.
 * @param ... Some va_args.
 */
void labcomm_error_to_ff_error(enum labcomm_error error_id, size_t nbr_va_args, ...);

#endif
