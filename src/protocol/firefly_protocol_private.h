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
 * @brief Error callback used in LabComm that used forwards the error the the
 * \e firefly_error handler that is in use.
 * @param error_id The LabComm error that occured.
 * @param nbr_va_args The number of va_args that are passed.
 * @param ... Some va_args.
 */
void labcomm_error_to_ff_error(enum labcomm_error error_id, size_t nbr_va_args,
									...);

/**
 * @brief Feeds LabComm decoder with bytes from the transport layer.
 * @param r The labcomm reader that requests data.
 * @param action What action is requested.
 * @return Value indicating how the action could be handled.
 */
int ff_transport_reader(labcomm_reader_t *r, labcomm_reader_action_t action);

/**
 * @brief Write bytes that LabComm has encoded to a memory and is then sent to
 * the transport layer.
 * @param w The labcomm writer that encodes the data.
 * @param action What action is requested.
 * @return Value indicating how the action could be handled.
 */
int ff_transport_writer(labcomm_writer_t *w, labcomm_writer_action_t action);

#endif
