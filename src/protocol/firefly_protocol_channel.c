#include "protocol/firefly_protocol_private.h"

#include <utils/firefly_errors.h>

struct firefly_channel *firefly_channel_new(struct firefly_connection *conn)
{
	struct firefly_channel   *chan;
	struct labcomm_decoder   *proto_decoder;
	struct labcomm_encoder   *proto_encoder;
	struct ff_transport_data *writer_data;
	struct ff_transport_data *reader_data;
	unsigned char            *writer_data_data;

	chan             = FIREFLY_MALLOC(sizeof(struct firefly_channel));
	proto_decoder    = labcomm_decoder_new(protocol_reader, chan);
	proto_encoder    = labcomm_encoder_new(protocol_writer, chan);
	reader_data      = FIREFLY_MALLOC(sizeof(struct ff_transport_data));
	writer_data      = FIREFLY_MALLOC(sizeof(struct ff_transport_data));
	writer_data_data = FIREFLY_MALLOC(BUFFER_SIZE);

	if (chan          == NULL || proto_decoder    == NULL ||
	    proto_encoder == NULL || reader_data      == NULL ||
	    writer_data   == NULL || writer_data_data == NULL)
	{
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
				"memory allocation failed");

		if (chan->proto_decoder)
			labcomm_decoder_free(chan->proto_decoder);
		if (chan->proto_encoder)
			labcomm_encoder_free(chan->proto_encoder);
		FIREFLY_FREE(reader_data);
		FIREFLY_FREE(writer_data);
		FIREFLY_FREE(writer_data_data);
		FIREFLY_FREE(chan);
		return NULL;
	}

	chan->local_id			= next_channel_id(conn);
	chan->remote_id			= CHANNEL_ID_NOT_SET;
	chan->important_queue	= NULL;
	chan->important_id		= 0;
	chan->current_seqno		= 0;
	chan->remote_seqno		= 0;
	chan->proto_decoder		= proto_decoder;
	chan->proto_encoder		= proto_encoder;

	chan->reader_data            = reader_data;
	chan->reader_data->data      = NULL;
	chan->reader_data->data_size = 0;
	chan->reader_data->pos       = 0;

	chan->writer_data            = writer_data;
	chan->writer_data->data      = writer_data_data;
	chan->writer_data->data_size = BUFFER_SIZE;
	chan->writer_data->pos       = 0;

	chan->conn = conn;

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
	if (chan->reader_data != NULL) {
		FIREFLY_FREE(chan->reader_data->data);
		FIREFLY_FREE(chan->reader_data);
	}
	if (chan->writer_data != NULL) {
		FIREFLY_FREE(chan->writer_data->data);
		FIREFLY_FREE(chan->writer_data);
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
