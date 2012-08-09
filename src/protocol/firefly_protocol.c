#include <protocol/firefly_protocol.h>
#include "protocol/firefly_protocol_private.h"

#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <labcomm.h>

#include <firefly_errors.h>
#include <gen/firefly_protocol.h>
#include "eventqueue/event_queue.h"


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
}

static void create_channel_closed_event(struct firefly_channel *chan,
						struct firefly_connection *conn)
{
	struct firefly_event_chan_closed *ev;
	int ret;

	ev = malloc(sizeof(struct firefly_event_chan_closed));
	if (ev == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}
	ev->base.type = EVENT_CHAN_CLOSED;
	ev->base.prio = 1;
	ev->conn = conn;
	ev->chan = chan;
	ret = conn->event_queue->offer_event_cb(conn->event_queue,
						(struct firefly_event *) ev);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
						"Could not add event to queue.");
	}
}

void firefly_channel_close(struct firefly_channel *chan,
						struct firefly_connection *conn)
{

	struct firefly_event_chan_close *ev;
	ev = malloc(sizeof(struct firefly_event_chan_close));
	if (ev == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not create event.");
	}
	ev->base.type = EVENT_CHAN_CLOSE;
	ev->base.prio = 1;
	ev->conn = conn;
	ev->chan_close = malloc(sizeof(firefly_protocol_channel_close));
	if (ev->chan_close == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not create event.");
	}
	ev->chan_close->dest_chan_id = chan->remote_id;
	ev->chan_close->source_chan_id = chan->local_id;
	int ret = conn->event_queue->offer_event_cb(conn->event_queue,
			(struct firefly_event *) ev);

	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not add event to queue.");
	}

	create_channel_closed_event(chan, conn);
}

void firefly_channel_close_event(struct firefly_connection *conn,
		firefly_protocol_channel_close *chan_close)
{
	labcomm_encode_firefly_protocol_channel_close(conn->transport_encoder,
			chan_close);
}

void firefly_channel_closed_event(struct firefly_channel *chan,
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
	struct firefly_event_chan_req_recv *ev =
			malloc(sizeof(struct firefly_event_chan_req_recv));
	if (ev == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}
	ev->base.type = EVENT_CHAN_REQ_RECV;
	ev->base.prio = 1;			/* not relevant yet */
	ev->conn = conn;
	ev->chan_req = malloc(sizeof(firefly_protocol_channel_request));
	memcpy(ev->chan_req, chan_req,
				sizeof(firefly_protocol_channel_request));

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
			firefly_channel_free(
				remove_channel_from_connection(chan, conn));
		}

		labcomm_encode_firefly_protocol_channel_response(
				conn->transport_encoder, &res);
	}
}

void handle_channel_response(firefly_protocol_channel_response *chan_res,
		void *context)
{
	int ret;
	struct firefly_connection *conn = (struct firefly_connection *) context;
	struct firefly_event_chan_res_recv *ev =
			malloc(sizeof(struct firefly_event_chan_res_recv));
	if (ev == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}
	ev->base.type = EVENT_CHAN_RES_RECV;
	ev->base.prio = 1;			/* not relevant yet */
	ev->conn = conn;
	ev->chan_res = malloc(sizeof(firefly_protocol_channel_response));
	memcpy(ev->chan_res, chan_res,
				sizeof(firefly_protocol_channel_response));

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
		labcomm_encode_firefly_protocol_channel_ack(
						conn->transport_encoder, &ack);
		// Should be done after encode above.
		conn->on_channel_opened(chan);
	} else {
		chan->on_chan_rejected(conn);
		firefly_channel_free(remove_channel_from_connection(chan,
									conn));
	}
}

void handle_channel_ack(firefly_protocol_channel_ack *chan_ack, void *context)
{
	int ret;
	struct firefly_connection *conn = (struct firefly_connection *) context;
	struct firefly_event_chan_ack_recv *ev =
			malloc(sizeof(struct firefly_event_chan_ack_recv));
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

size_t firefly_number_channels_in_connection(struct firefly_connection *conn)
{
	struct channel_list_node *node;
	size_t n_chan = 0;

	node = conn->chan_list;
	if (node) {
		n_chan++;
		while ((node = node->next) != NULL)
			n_chan++;
	}

	return n_chan;

}
