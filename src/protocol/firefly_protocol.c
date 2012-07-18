#include <protocol/firefly_protocol.h>
#include "protocol/firefly_protocol_private.h"

#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <labcomm.h>

#include <firefly_errors.h>
#include <gen/firefly_protocol.h>

#define BUFFER_SIZE (128)

void firefly_channel_open(struct firefly_connection *conn)
{
	// TODO implement better
	firefly_protocol_channel_request chan_req;
	chan_req.source_chan_id = 1;
	chan_req.dest_chan_id = 0;
	chan_req.ack = true;

	conn->writer_data->data = malloc(128);
	conn->writer_data->data_size = 128;
	conn->writer_data->pos = 0;

	struct firefly_channel *chan = malloc(sizeof(struct firefly_channel));
	chan->local_id = 1;
	chan->remote_id = -1;
	add_channel_to_connection(chan, conn);

	labcomm_encode_firefly_protocol_channel_request(conn->transport_encoder,
			&chan_req);
}

void firefly_channel_close(struct firefly_channel **chan,
		struct firefly_connection *conn)
{
	// TODO implement better, quick and dirty implementation to make the
	// tests mem leak free

	struct channel_list_node **head = &conn->chan_list;
	while (*head != NULL) {
		if ((*head)->chan->local_id == (*chan)->local_id) {
			struct channel_list_node *tmp = (*head)->next;
			if ((*head)->chan->proto_encoder != NULL) {
				labcomm_encoder_free((*head)->chan->proto_encoder);
			}
			if ((*head)->chan->proto_decoder != NULL) {
				labcomm_decoder_free((*head)->chan->proto_decoder);
			}
			free((*head)->chan);
			free(*head);
			*head = tmp;
		} else {
			*head = (*head)->next;
		}
	}
}

void protocol_data_received(struct firefly_connection *conn,
		unsigned char *data, size_t size)
{
	conn->reader_data->data = data;
	conn->reader_data->data_size = size;
	conn->reader_data->pos = 0;
	labcomm_decoder_decode_one(conn->transport_decoder);

}

void handle_channel_request(firefly_protocol_channel_request *cr, void *context)
{
	// TODO check if req, req_ack or ack
	int local_chan_id = cr->dest_chan_id;
	struct firefly_channel *chan = find_channel_by_local_id(local_chan_id,
			(struct firefly_connection *) context);
	if (chan != NULL) {
		chan->remote_id = cr->source_chan_id;
		protocol_channel_request_send_ack(chan,
			(struct firefly_connection *) context);
		((struct firefly_connection *)
					context)->on_channel_opened(chan);
	}
}

void protocol_channel_request_send_ack(struct firefly_channel *chan,
		struct firefly_connection *conn)
{
	firefly_protocol_channel_request chan_req;
	chan_req.source_chan_id = chan->local_id;
	chan_req.dest_chan_id = chan->remote_id;
	chan_req.ack = true;
	conn->writer_data->pos = 0;
	labcomm_encode_firefly_protocol_channel_request(conn->transport_encoder,
			&chan_req);
}

void add_channel_to_connection(struct firefly_channel *chan,
		struct firefly_connection *conn)
{
	struct channel_list_node *new_node =
		malloc(sizeof(struct channel_list_node));
	new_node->chan = chan;
	new_node->next = conn->chan_list;
	conn->chan_list = new_node;
}

struct firefly_channel *find_channel_by_local_id(int id,
		struct firefly_connection *conn)
{
	struct channel_list_node *head = conn->chan_list;
	while (head != NULL) {
		if (head->chan->local_id == id) {
			break;
		}
		head = head->next;
	}
	return head == NULL ? NULL : head->chan;
}

struct labcomm_encoder *firefly_protocol_get_output_stream(
				struct firefly_channel *chan)
{
	return chan->proto_encoder;
}

struct labcomm_decoder *firefly_protocol_get_input_stream(
				struct firefly_channel *chan)
{
	return chan->proto_decoder;
}

// TODO use this for errors.
void labcomm_error_to_ff_error(enum labcomm_error error_id, size_t nbr_va_args,
									...)
{
	const char *err_msg = labcomm_error_get_str(error_id);
	if (err_msg == NULL) {
		err_msg = "Error with an unknown error ID occured.";
	}
	firefly_error(FIREFLY_ERROR_LABCOMM, 1, err_msg);
}

static int copy_to_writer_data(struct ff_transport_data *writer_data,
		unsigned char *data, size_t size)
{
	// TODO Consider alternative ways to prevent labcomm packages
	// to be fragmented.
	if (writer_data->data_size - writer_data->pos < size) {
		size_t new_size = writer_data->data_size * 2;
		unsigned char *tmp = realloc(writer_data->data, new_size);
		if (tmp == NULL) {
			return -1;
		}
		writer_data->data = tmp;
		writer_data->data_size = new_size;
	}
	memcpy(&writer_data->data[writer_data->pos], data, size);
	writer_data->pos += size;
	return 0;
}

int firefly_labcomm_reader(labcomm_reader_t *r, labcomm_reader_action_t action,
		struct ff_transport_data *reader_data)
{
	int result = -EINVAL;
	switch (action) {
	case labcomm_reader_alloc: {
		r->data = malloc(BUFFER_SIZE);
		if (r->data == NULL) {
			result = ENOMEM;
		} else {
			r->data_size = BUFFER_SIZE;
			r->pos = 0;
			r->count = 0;
		}
	} break;
	case labcomm_reader_free: {
		free(r->data);
		r->data = NULL;
		r->data_size = 0;
		r->pos = 0;
		r->count = 0;
	} break;
	case labcomm_reader_start:
	case labcomm_reader_continue: {
		r->pos = 0;
		size_t data_left = reader_data->data_size - reader_data->pos;
		if (data_left <= 0) {
			result = -1; // Stop.
		} else {
			size_t reader_avail = r->data_size;
			size_t mem_to_cpy = (data_left < reader_avail) ?
						data_left : reader_avail;
			memcpy(r->data, &reader_data->data[reader_data->pos],
					mem_to_cpy);
			reader_data->pos += mem_to_cpy;
			r->count = mem_to_cpy;
			result = mem_to_cpy;
		}
	} break;
	case labcomm_reader_end: {
		size_t data_left = reader_data->data_size - reader_data->pos;
		if (data_left <= 0) {
			result = -1;
		} else {
			result = 0;
		}
	} break;
	}
	return result;
}

int ff_transport_writer(labcomm_writer_t *w, labcomm_writer_action_t action)
{
	struct firefly_connection  *conn =
			(struct firefly_connection *) w->context;
	struct ff_transport_data *writer_data = conn->writer_data;
	int result = -EINVAL;
	switch (action) {
	case labcomm_writer_alloc: {
		w->data = malloc(BUFFER_SIZE);
		if (w->data == NULL) {
			w->data_size = 0;
			w->count = 0;
			w->on_error(LABCOMM_ERROR_MEMORY, 1,
					"Writer could not allocate memory.\n");
			result = -ENOMEM;
		} else {
			w->data_size = BUFFER_SIZE;
			w->count = BUFFER_SIZE;
			w->pos = 0;
		}
   	} break;
	case labcomm_writer_free: {
		free(w->data);
		w->data = NULL;
		w->data_size = 0;
		w->count = 0;
		w->pos = 0;
	} break;
	case labcomm_writer_start: {
		w->pos = 0;
	} break;
	case labcomm_writer_continue: {
		int res = copy_to_writer_data(writer_data, w->data, w->pos);
		if (res == -1) {
			w->on_error(LABCOMM_ERROR_MEMORY, 1,
				"Writer could not save encoded data from "
								"labcomm\n");
			result = -ENOMEM;
		} else {
			w->pos = 0;
			result = 0;
		}
	} break;
	case labcomm_writer_end: {
		int res = copy_to_writer_data(writer_data, w->data, w->pos);
		if (res == -1) {
			w->on_error(LABCOMM_ERROR_MEMORY, 1,
				"Writer could not save encoded data from "
								"labcomm\n");
			result = -ENOMEM;
		} else {
			w->pos = 0;
			result = 0;
			conn->transport_write(writer_data->data,
						writer_data->pos, conn);
		}
	} break;
	case labcomm_writer_available: {
		result = w->count - w->pos;
	} break;
	}
	return result;
}

int ff_transport_reader(labcomm_reader_t *r, labcomm_reader_action_t action)
{
	struct firefly_connection *conn =
			(struct firefly_connection *) r->context;
	struct ff_transport_data *reader_data = conn->reader_data;
	return firefly_labcomm_reader(r, action, reader_data);
}

int protocol_writer(labcomm_writer_t *w, labcomm_writer_action_t action)
{
	struct firefly_channel *chan =
			(struct firefly_channel *) w->context;
	struct ff_transport_data *writer_data = chan->writer_data;
	int result = -EINVAL;
	switch (action) {
	case labcomm_writer_alloc: {
		w->data = malloc(BUFFER_SIZE);
		if (w->data == NULL) {
			w->data_size = 0;
			w->count = 0;
			w->on_error(LABCOMM_ERROR_MEMORY, 1,
					"Writer could not allocate memory.\n");
			result = -ENOMEM;
		} else {
			w->data_size = BUFFER_SIZE;
			w->count = BUFFER_SIZE;
			w->pos = 0;
		}
   	} break;
	case labcomm_writer_free: {
		free(w->data);
		w->data = NULL;
		w->data_size = 0;
		w->count = 0;
		w->pos = 0;
	} break;
	case labcomm_writer_start: {
		w->pos = 0;
	} break;
	case labcomm_writer_continue: {
		int res = copy_to_writer_data(writer_data, w->data, w->pos);
		if (res == -1) {
			w->on_error(LABCOMM_ERROR_MEMORY, 1,
				"Writer could not save encoded data from "
								"labcomm\n");
			result = -ENOMEM;
		} else {
			w->pos = 0;
			result = 0;
		}
	} break;
	case labcomm_writer_end: {
		int res = copy_to_writer_data(writer_data, w->data, w->pos);
		if (res == -1) {
			w->on_error(LABCOMM_ERROR_MEMORY, 1,
				"Writer could not save encoded data from "
								"labcomm\n");
			result = -ENOMEM;
		} else {
			w->pos = 0;
			result = 0;
			// create protocol packet and encode it
			firefly_protocol_data_sample pkt;
			pkt.dest_chan_id = chan->remote_id;
			pkt.src_chan_id = chan->local_id;
			pkt.important = true;
			pkt.app_enc_data.a = writer_data->data;
			pkt.app_enc_data.n_0 = writer_data->pos;
			labcomm_encode_firefly_protocol_data_sample(
					chan->conn->transport_encoder, &pkt);
			writer_data->pos = 0;
		}
	} break;
	case labcomm_writer_available: {
		result = w->count - w->pos;
	} break;
	}
	return result;
}

int protocol_reader(labcomm_reader_t *r, labcomm_reader_action_t action)
{
	struct firefly_channel *chan =
			(struct firefly_channel *) r->context;
	struct ff_transport_data *reader_data = chan->reader_data;
	return firefly_labcomm_reader(r, action, reader_data);
}
