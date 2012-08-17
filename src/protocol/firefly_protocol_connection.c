#include <protocol/firefly_protocol.h>
#include "protocol/firefly_protocol_private.h"

#include <utils/firefly_errors.h>

struct firefly_connection *firefly_connection_new_register(
		firefly_channel_is_open_f on_channel_opened,
		firefly_channel_closed_f on_channel_closed,
		firefly_channel_accept_f on_channel_recv,
		transport_write_f transport_write,
		struct firefly_event_queue *event_queue, void *plat_spec, bool reg)
{
	struct firefly_connection *conn =
		malloc(sizeof(struct firefly_connection));
	if (conn == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "malloc failed\n");
	}
	conn->event_queue = event_queue;
	// Init writer data
	struct ff_transport_data *writer_data =
				malloc(sizeof(struct ff_transport_data));
	if (writer_data == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "malloc failed\n");
	}
	writer_data->data = malloc(BUFFER_SIZE);
	if (writer_data->data == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "malloc failed\n");
	}
	writer_data->data_size = BUFFER_SIZE;
	writer_data->pos = 0;
	conn->writer_data = writer_data;

	// Init reader data
	struct ff_transport_data *reader_data =
				malloc(sizeof(struct ff_transport_data));
	if (reader_data == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "malloc failed\n");
	}
	reader_data->data = NULL;
	reader_data->data_size = 0;
	reader_data->pos = 0;
	conn->reader_data = reader_data;

	conn->on_channel_opened = on_channel_opened;
	conn->on_channel_closed = on_channel_closed;
	conn->on_channel_recv = on_channel_recv;
	conn->chan_list = NULL;
	conn->channel_id_counter = 0;
	conn->transport_encoder =
		labcomm_encoder_new(ff_transport_writer, conn);
	if (conn->transport_encoder == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "malloc failed\n");
	}
	labcomm_register_error_handler_encoder(conn->transport_encoder,
			labcomm_error_to_ff_error);

	conn->transport_decoder =
		labcomm_decoder_new(ff_transport_reader, conn);
	if (conn->transport_decoder == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "malloc failed\n");
	}
	labcomm_register_error_handler_decoder(conn->transport_decoder,
			labcomm_error_to_ff_error);

	conn->transport_write = transport_write;
	conn->transport_conn_platspec = plat_spec;

	if (reg) {
	reg_proto_sigs(conn->transport_encoder,
				   conn->transport_decoder,
				   conn);
	}

	return conn;
}

struct firefly_connection *firefly_connection_new(
		firefly_channel_is_open_f on_channel_opened,
		firefly_channel_closed_f on_channel_closed,
		firefly_channel_accept_f on_channel_recv,
		transport_write_f transport_write,
		struct firefly_event_queue *event_queue,
		void *plat_spec)
{
	return firefly_connection_new_register(on_channel_opened,
										   on_channel_closed,
										   on_channel_recv,
										   transport_write,
										   event_queue,
										   plat_spec,
										   false);
}

void firefly_connection_free(struct firefly_connection **conn)
{
	while ((*conn)->chan_list != NULL) {
		firefly_channel_closed_event((*conn)->chan_list->chan);
		/*firefly_channel_free(remove_channel_from_connection(*/
					/*(*conn)->chan_list->chan, *conn));*/
	}
	free((*conn)->chan_list);
	if ((*conn)->transport_encoder != NULL) {
		labcomm_encoder_free((*conn)->transport_encoder);
	}
	if ((*conn)->transport_decoder != NULL) {
		labcomm_decoder_free((*conn)->transport_decoder);
	}
	free((*conn)->reader_data);
	free((*conn)->writer_data->data);
	free((*conn)->writer_data);
	free((*conn));
	*conn = NULL;
}

struct firefly_channel *remove_channel_from_connection(
		struct firefly_channel *chan, struct firefly_connection *conn)
{
	struct firefly_channel *ret = NULL;
	struct channel_list_node **head = &conn->chan_list;
	while (head !=NULL && (*head) != NULL) {
		if ((*head)->chan->local_id == chan->local_id) {
			ret = chan;
			struct channel_list_node *tmp = (*head)->next;
			free(*head);
			*head = tmp;
			head = NULL;
		} else {
			*head = (*head)->next;
		}
	}
	return ret;
}

int next_channel_id(struct firefly_connection *conn)
{
	return conn->channel_id_counter++;
}
