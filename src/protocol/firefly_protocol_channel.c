#include "protocol/firefly_protocol_private.h"

#include <utils/firefly_errors.h>

struct firefly_channel *firefly_channel_new(struct firefly_connection *conn)
{
	struct firefly_channel *chan;
	struct labcomm_decoder *proto_decoder;
	struct labcomm_encoder *proto_encoder;
	struct labcomm_reader  *reader;
	struct labcomm_writer  *writer;

	chan             = FIREFLY_MALLOC(sizeof(struct firefly_channel));
	if (chan == NULL) {

		firefly_error(FIREFLY_ERROR_ALLOC, 1,
				"memory allocation failed");

		FIREFLY_FREE(chan);
		return NULL;
	}
	reader = protocol_labcomm_reader_new();
	writer = protocol_labcomm_writer_new(chan);
	if (reader == NULL || writer == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
				"memory allocation failed");
		protocol_labcomm_reader_free(reader);
		protocol_labcomm_writer_free(writer);
		FIREFLY_FREE(chan);
		return NULL;
	}

	proto_decoder    = labcomm_decoder_new(reader, NULL);
	proto_encoder    = labcomm_encoder_new(writer, NULL);

	if (proto_decoder == NULL || proto_encoder == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
				"memory allocation failed");

		if (chan->proto_decoder)
			labcomm_decoder_free(chan->proto_decoder);
		if (chan->proto_encoder)
			labcomm_encoder_free(chan->proto_encoder);
		FIREFLY_FREE(chan);
		return NULL;
	}

	labcomm_register_error_handler_encoder(proto_encoder,
			labcomm_error_to_ff_error);
	labcomm_register_error_handler_decoder(proto_decoder,
			labcomm_error_to_ff_error);

	chan->local_id			= next_channel_id(conn);
	chan->remote_id			= CHANNEL_ID_NOT_SET;
	chan->important_queue	= NULL;
	chan->important_id		= 0;
	chan->current_seqno		= 0;
	chan->remote_seqno		= 0;
	chan->proto_decoder		= proto_decoder;
	chan->proto_encoder		= proto_encoder;
	chan->conn              = conn;

	return chan;
}

void firefly_channel_free(struct firefly_channel *chan)
{
	if (chan == NULL) {
		return;
	}

	if (chan->proto_decoder != NULL) {
		labcomm_decoder_free(chan->proto_decoder);
	}
	if (chan->proto_encoder != NULL) {
		labcomm_encoder_free(chan->proto_encoder);
	}
	FIREFLY_FREE(chan);
}

struct firefly_connection *firefly_channel_get_connection(
		struct firefly_channel *chan)
{
	return chan->conn;
}

int firefly_channel_next_seqno(struct firefly_channel *chan)
{
	if (++chan->current_seqno <= 0) {
		chan->current_seqno = 1;
	}
	return chan->current_seqno;
}

int firefly_channel_closed_event(void *event_arg)
{
	struct firefly_channel *chan = (struct firefly_channel *) event_arg;

	remove_channel_from_connection(chan, chan->conn);
	if (chan->conn->on_channel_closed != NULL) {
		chan->conn->on_channel_closed(chan);
	}
	firefly_channel_free(chan);

	return 0;
}
