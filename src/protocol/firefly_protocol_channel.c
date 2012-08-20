#include "protocol/firefly_protocol_private.h"

#include <utils/firefly_errors.h>

struct firefly_channel *firefly_channel_new(struct firefly_connection *conn)
{
	struct firefly_channel *chan = malloc(sizeof(struct firefly_channel));

	if (chan != NULL) {
		chan->local_id = next_channel_id(conn);
		chan->remote_id = CHANNEL_ID_NOT_SET;
		chan->proto_decoder = labcomm_decoder_new(protocol_reader, chan);
		chan->proto_encoder = labcomm_encoder_new(protocol_writer, chan);
		if (chan->proto_encoder == NULL || chan->proto_decoder == NULL) {
			firefly_error(FIREFLY_ERROR_ALLOC, 1, "Labcomm new failed\n");
		}
		chan->reader_data = malloc(sizeof(*chan->reader_data));
		chan->reader_data->data = NULL;
		chan->reader_data->data_size = 0;
		chan->reader_data->pos = 0;
		if (chan->reader_data == NULL) {
			firefly_error(FIREFLY_ERROR_ALLOC, 1, "malloc failed\n");
		}
		chan->writer_data = malloc(sizeof(*chan->writer_data));
		if (chan->writer_data == NULL) {
			firefly_error(FIREFLY_ERROR_ALLOC, 1, "malloc failed\n");
		}
		chan->writer_data->data = malloc(BUFFER_SIZE);
		chan->writer_data->data_size = BUFFER_SIZE;
		chan->writer_data->pos = 0;
		chan->conn = conn;
	} else {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "malloc failed");
	}

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
		free(chan->reader_data->data);
		free(chan->reader_data);
	}
	if (chan->writer_data != NULL) {
		free(chan->writer_data->data);
		free(chan->writer_data);
	}
	free(chan);
}

struct firefly_connection *firefly_channel_get_connection(
		struct firefly_channel *chan)
{
	return chan->conn;
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
