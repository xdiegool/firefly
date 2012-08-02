#include <protocol/firefly_protocol.h>
#include "protocol/firefly_protocol_private.h"

#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <labcomm.h>

#include <firefly_errors.h>
#include <gen/firefly_protocol.h>
#include "eventqueue/event_queue.h"

#define BUFFER_SIZE (128)

struct firefly_connection *firefly_connection_new(
		firefly_channel_is_open_f on_channel_opened,
		firefly_channel_closed_f on_channel_closed,
		firefly_channel_accept_f on_channel_recv,
		struct firefly_event_queue *event_queue)
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

	conn->transport_decoder =
		labcomm_decoder_new(ff_transport_reader, conn);
	if (conn->transport_decoder == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "malloc failed\n");
	}

	return conn;
}

void firefly_connection_free(struct firefly_connection **conn)
{
	if ((*conn)->transport_encoder != NULL) {
		labcomm_encoder_free((*conn)->transport_encoder);
	}
	if ((*conn)->transport_decoder != NULL) {
		labcomm_decoder_free((*conn)->transport_decoder);
	}
	free((*conn)->reader_data);
	free((*conn)->writer_data->data);
	free((*conn)->writer_data);
	while ((*conn)->chan_list != NULL) {
		firefly_channel_free(remove_channel_from_connection(
					(*conn)->chan_list->chan, *conn));
	}
	free((*conn)->chan_list);
	free((*conn));
	*conn = NULL;
}

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
	}

	return chan;
}

// TODO consider remove pointer to pointer
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

void firefly_channel_open(struct firefly_connection *conn,
		firefly_channel_rejected_f on_chan_rejected)
{
	struct firefly_event_chan_open *ev;
	int ret;
	/* create event. add to q. */

	ev = malloc(sizeof(struct firefly_event_chan_open));
	if (ev == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}
	ev->base.type = EVENT_CHAN_OPEN;
	ev->base.prio = 1;			/* not relevant yet */
	ev->conn = conn;
	ev->rejected_cb = on_chan_rejected;

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
											(struct firefly_event *) ev);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "could not add event to queue");
	}
}

void firefly_channel_open_event(struct firefly_connection *conn,
		firefly_channel_rejected_f on_chan_rejected)
{
	// TODO implement better
	struct firefly_channel *chan = firefly_channel_new(conn);
	if (chan == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate channel.\n");
	}
	chan->on_chan_rejected = on_chan_rejected;
	add_channel_to_connection(chan, conn);

	firefly_protocol_channel_request chan_req;
	chan_req.source_chan_id = chan->local_id;
	chan_req.dest_chan_id = chan->remote_id;

	labcomm_encode_firefly_protocol_channel_request(conn->transport_encoder,
			&chan_req);
	conn->writer_data->pos = 0;
}

static void create_channel_closed_event(struct firefly_channel *chan,
								struct firefly_connection *conn)
{
	struct firefly_event_chan_close *ev;
	int ret;

	ev = malloc(sizeof(struct firefly_event_chan_close));
	if (ev == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}
	ev->base.type = EVENT_CHAN_CLOSE;
	ev->base.prio = 1;
	ev->conn = conn;
	ev->chan = chan;
	ret = conn->event_queue->offer_event_cb(conn->event_queue,
											(struct firefly_event *) ev);
	if (ret)
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "could not add event to queue");
}

void firefly_channel_close(struct firefly_channel *chan,
						   struct firefly_connection *conn)
{
	create_channel_closed_event(chan, conn);

	firefly_protocol_channel_close chan_close;
	chan_close.dest_chan_id = chan->remote_id;
	chan_close.source_chan_id = chan->local_id;
	labcomm_encode_firefly_protocol_channel_close(conn->transport_encoder,
			&chan_close);
	conn->writer_data->pos = 0;
}

void firefly_channel_close_event(struct firefly_channel *chan,
								 struct firefly_connection *conn)
{
	if (conn->on_channel_closed != NULL) {
		conn->on_channel_closed(chan);
	}
	firefly_channel_free(remove_channel_from_connection(chan, conn));
}

void protocol_data_received(struct firefly_connection *conn,
		unsigned char *data, size_t size)
{
	conn->reader_data->data = data;
	conn->reader_data->data_size = size;
	conn->reader_data->pos = 0;
	labcomm_decoder_decode_one(conn->transport_decoder);
	conn->reader_data->data = NULL;
	conn->reader_data->data_size = 0;
	conn->reader_data->pos = 0;
}

void handle_channel_request(firefly_protocol_channel_request *chan_req,
		void *context)
{
	int ret;
	struct firefly_connection *conn = (struct firefly_connection *) context;
	struct firefly_event_chan_req_recv *ev = malloc(sizeof(struct firefly_event_chan_req_recv));
	if (ev == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}
	ev->base.type = EVENT_CHAN_REQ_RECV;
	ev->base.prio = 1;			/* not relevant yet */
	ev->conn = conn;
	ev->chan_req = malloc(sizeof(firefly_protocol_channel_request));
	memcpy(ev->chan_req, chan_req, sizeof(firefly_protocol_channel_request));

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
											(struct firefly_event *) ev);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "could not add event to queue");
	}

}

void handle_channel_request_event(firefly_protocol_channel_request *chan_req,
		struct firefly_connection *conn)
{
	int local_chan_id = chan_req->dest_chan_id;
	struct firefly_channel *chan = find_channel_by_local_id(conn,
			local_chan_id);
	if (chan != NULL) {
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1, "Received open"
						"channel on existing channel.\n");
	} else {
		// Received Channel request.
		chan = firefly_channel_new(conn);
		if (chan == NULL) {
			firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not"
							"allocate channel.\n");
		}
		add_channel_to_connection(chan, conn);

		chan->remote_id = chan_req->source_chan_id;

		firefly_protocol_channel_response res;
		res.dest_chan_id = chan->remote_id;
		res.source_chan_id = chan->local_id;
		res.ack = conn->on_channel_recv(chan);
		if (!res.ack) {
			res.source_chan_id = CHANNEL_ID_NOT_SET;
			firefly_channel_free(remove_channel_from_connection(chan, conn));
		}

		labcomm_encode_firefly_protocol_channel_response(
				conn->transport_encoder, &res);
		conn->writer_data->pos = 0;
	}
}

void handle_channel_response(firefly_protocol_channel_response *chan_res,
		void *context)
{
	int ret;
	struct firefly_connection *conn = (struct firefly_connection *) context;
	struct firefly_event_chan_res_recv *ev = malloc(sizeof(struct firefly_event_chan_res_recv));
	if (ev == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}
	ev->base.type = EVENT_CHAN_RES_RECV;
	ev->base.prio = 1;			/* not relevant yet */
	ev->conn = conn;
	ev->chan_res = malloc(sizeof(firefly_protocol_channel_response));
	memcpy(ev->chan_res, chan_res, sizeof(firefly_protocol_channel_response));

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
											(struct firefly_event *) ev);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "could not add event to queue");
	}

}

void handle_channel_response_event(firefly_protocol_channel_response *chan_res,
		struct firefly_connection *conn)
{
	int local_chan_id = chan_res->dest_chan_id;
	struct firefly_channel *chan = find_channel_by_local_id(conn,
			local_chan_id);

	firefly_protocol_channel_ack ack;
	ack.dest_chan_id = chan_res->source_chan_id;
	if (chan != NULL) {
		// Received Channel request ack.
		chan->remote_id = chan_res->source_chan_id;
		ack.source_chan_id = chan->local_id;
		ack.ack = true;
	} else {
		ack.source_chan_id = CHANNEL_ID_NOT_SET;
		ack.ack = false;
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1, "Received open"
						"channel on non-existing"
						"channel");
	}
	if (chan_res->ack) {
		labcomm_encode_firefly_protocol_channel_ack(conn->transport_encoder,
							&ack);
		conn->writer_data->pos = 0;
		// Should be done after encode above.
		conn->on_channel_opened(chan);
	} else {
		chan->on_chan_rejected(conn);
		firefly_channel_free(remove_channel_from_connection(chan, conn));
	}
}

void handle_channel_ack(firefly_protocol_channel_ack *chan_ack, void *context)
{
	int ret;
	struct firefly_connection *conn = (struct firefly_connection *) context;
	struct firefly_event_chan_ack_recv *ev = malloc(sizeof(struct firefly_event_chan_ack_recv));
	if (ev == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}
	ev->base.type = EVENT_CHAN_ACK_RECV;
	ev->base.prio = 1;			/* not relevant yet */
	ev->conn = conn;
	ev->chan_ack = malloc(sizeof(firefly_protocol_channel_ack));
	memcpy(ev->chan_ack, chan_ack, sizeof(firefly_protocol_channel_ack));

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
											(struct firefly_event *) ev);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "could not add event to queue");
	}
}

void handle_channel_ack_event(firefly_protocol_channel_ack *chan_ack,
		struct firefly_connection *conn)
{
	int local_chan_id = chan_ack->dest_chan_id;
	struct firefly_channel *chan = find_channel_by_local_id(conn,
			local_chan_id);

	if (chan != NULL) {
		conn->on_channel_opened(chan);
	} else {
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1, "Received ack"
						"on non-existing channel.\n");
	}
}

void handle_channel_close(firefly_protocol_channel_close *chan_close,
		void *context)
{
	struct firefly_connection *conn = (struct firefly_connection *) context;
	struct firefly_channel *chan = find_channel_by_local_id(
			conn, chan_close->dest_chan_id);
	if (chan != NULL){
		create_channel_closed_event(chan, conn);
	} else {
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1,
				"Received closed on non-existing channel.\n");
	}
}

void handle_data_sample(firefly_protocol_data_sample *data,
		void *context)
{
	struct firefly_connection *conn = (struct firefly_connection *) context;
	firefly_protocol_data_sample *pkt =
		malloc(sizeof(firefly_protocol_data_sample));
	memcpy(pkt, data, sizeof(firefly_protocol_data_sample));
	pkt->app_enc_data.a = malloc(data->app_enc_data.n_0);
	memcpy(pkt->app_enc_data.a, data->app_enc_data.a, data->app_enc_data.n_0);
	struct firefly_event_recv_sample *ev =
		malloc(sizeof(struct firefly_event_recv_sample));
	ev->base.type = EVENT_RECV_SAMPLE;
	ev->base.prio = 1;
	ev->conn = conn;
	ev->pkt = pkt;
	conn->event_queue->offer_event_cb(conn->event_queue,
			(struct firefly_event *) ev);
}

void handle_data_sample_event(firefly_protocol_data_sample *data,
		struct firefly_connection *conn)
{
	struct firefly_channel *chan = find_channel_by_local_id(conn,
			data->dest_chan_id);
	chan->reader_data->data = data->app_enc_data.a;
	chan->reader_data->data_size = data->app_enc_data.n_0;
	chan->reader_data->pos = 0;
	labcomm_decoder_decode_one(chan->proto_decoder);
	chan->reader_data->data = NULL;
	chan->reader_data->data_size = 0;
	chan->reader_data->pos = 0;
}

struct firefly_channel *find_channel_by_local_id(struct firefly_connection *conn,
		int id)
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

void add_channel_to_connection(struct firefly_channel *chan,
		struct firefly_connection *conn)
{
	struct channel_list_node *new_node =
		malloc(sizeof(struct channel_list_node));
	new_node->chan = chan;
	new_node->next = conn->chan_list;
	conn->chan_list = new_node;
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
				"Transport writer could not save encoded data from "
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
				"Transport writer could not save encoded data from "
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
				"Protocol writer could not save encoded data from "
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
				"Protocol writer could not save encoded data from "
								"labcomm\n");
			result = -ENOMEM;
		} else {
			w->pos = 0;
			result = 0;
			// create protocol packet and encode it
			struct firefly_event_send_sample *ev =
				malloc(sizeof(struct firefly_event_send_sample));
			firefly_protocol_data_sample *pkt =
				malloc(sizeof(firefly_protocol_data_sample));
			unsigned char *a = malloc(writer_data->pos);
			if (ev == NULL || pkt == NULL || a == NULL) {
				firefly_error(FIREFLY_ERROR_ALLOC, 1,
						"Protocol writer could not allocate send event\n");
			}
			ev->base.type = EVENT_SEND_SAMPLE;
			ev->base.prio = 1;
			ev->conn = chan->conn;
			ev->pkt = pkt;
			ev->pkt->dest_chan_id = chan->remote_id;
			ev->pkt->src_chan_id = chan->local_id;
			ev->pkt->important = true;
			ev->pkt->app_enc_data.n_0 = writer_data->pos;
			ev->pkt->app_enc_data.a = a;
			memcpy(ev->pkt->app_enc_data.a, writer_data->data,
					writer_data->pos);
			chan->conn->event_queue->offer_event_cb(chan->conn->event_queue,
					(struct firefly_event *) ev);
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
