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
	struct firefly_event_chan_open *feco;
	int ret;
	/* create event. add to q. */

	feco = malloc(sizeof(struct firefly_event_chan_open));
	if (feco == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}
	feco->conn = conn;
	feco->rejected_cb = on_chan_rejected;
	struct firefly_event *ev = firefly_event_new(1, firefly_channel_open_event,
			feco);

	ret = conn->event_queue->offer_event_cb(conn->event_queue, ev);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "could not add event to queue");
	}
}

int firefly_channel_open_event(void *event_arg)
{
	struct firefly_event_chan_open *feco =
		(struct firefly_event_chan_open *) event_arg;
	struct firefly_connection *conn = feco->conn;
	firefly_channel_rejected_f on_chan_rejected = feco->rejected_cb;
	struct firefly_channel *chan = firefly_channel_new(conn);
	if (chan == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate channel.\n");
		free(event_arg);
		return -1;
	}
	chan->on_chan_rejected = on_chan_rejected;
	add_channel_to_connection(chan, conn);

	firefly_protocol_channel_request chan_req;
	chan_req.source_chan_id = chan->local_id;
	chan_req.dest_chan_id = chan->remote_id;

	labcomm_encode_firefly_protocol_channel_request(conn->transport_encoder,
			&chan_req);
	free(event_arg);
	return 0;
}

static void create_channel_closed_event(struct firefly_channel *chan)
{
	int ret;

	struct firefly_event *ev = firefly_event_new(1,
			firefly_channel_closed_event, chan);
	ret = chan->conn->event_queue->offer_event_cb(chan->conn->event_queue, ev);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
						"Could not add event to queue.");
	}
}

void firefly_channel_close(struct firefly_channel *chan)
{

	struct firefly_event_chan_close *fecc;
	fecc = malloc(sizeof(struct firefly_event_chan_close));
	if (fecc == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not create event.");
	}
	fecc->conn = chan->conn;
	fecc->chan_close.dest_chan_id = chan->remote_id;
	fecc->chan_close.source_chan_id = chan->local_id;
	struct firefly_event *ev = firefly_event_new(1, firefly_channel_close_event,
			fecc);
	int ret = chan->conn->event_queue->offer_event_cb(chan->conn->event_queue,
			ev);

	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not add event to queue.");
	}

	create_channel_closed_event(chan);
}

int firefly_channel_close_event(void *event_arg)
{
	struct firefly_event_chan_close *fecc =
			(struct firefly_event_chan_close *) event_arg;
	labcomm_encode_firefly_protocol_channel_close(fecc->conn->transport_encoder,
			&fecc->chan_close);
	free(event_arg);
	return 0;
}

// Add const to data?
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
	struct firefly_event_chan_req_recv *fecrr =
			malloc(sizeof(struct firefly_event_chan_req_recv));
	if (fecrr == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}
	fecrr->conn = conn;
	memcpy(&fecrr->chan_req, chan_req,
				sizeof(firefly_protocol_channel_request));
	struct firefly_event *ev = firefly_event_new(1,
			handle_channel_request_event, fecrr);

	ret = conn->event_queue->offer_event_cb(conn->event_queue, ev);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "could not add event to queue");
	}

}

int handle_channel_request_event(void *event_arg)
{
	int res = 0;
	struct firefly_event_chan_req_recv *fecrr =
		(struct firefly_event_chan_req_recv *) event_arg;
	int local_chan_id = fecrr->chan_req.dest_chan_id;
	struct firefly_channel *chan = find_channel_by_local_id(fecrr->conn,
			local_chan_id);
	if (chan != NULL) {
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1, "Received open"
					"channel on existing channel.\n");
		res = -1;
	} else {
		// Received Channel request.
		chan = firefly_channel_new(fecrr->conn);
		if (chan == NULL) {
			firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not"
						"allocate channel.\n");
			res = -1;
		} else {
			add_channel_to_connection(chan, fecrr->conn);

			chan->remote_id = fecrr->chan_req.source_chan_id;

			firefly_protocol_channel_response res;
			res.dest_chan_id = chan->remote_id;
			res.source_chan_id = chan->local_id;
			res.ack = fecrr->conn->on_channel_recv(chan);
			if (!res.ack) {
				res.source_chan_id = CHANNEL_ID_NOT_SET;
				firefly_channel_free(
					remove_channel_from_connection(chan, fecrr->conn));
			}

			labcomm_encode_firefly_protocol_channel_response(
					fecrr->conn->transport_encoder, &res);
		}
	}
	free(event_arg);
	return res;
}

void handle_channel_response(firefly_protocol_channel_response *chan_res,
		void *context)
{
	int ret;
	struct firefly_connection *conn = (struct firefly_connection *) context;
	struct firefly_event_chan_res_recv *fecrr =
			malloc(sizeof(struct firefly_event_chan_res_recv));
	if (fecrr == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}
	fecrr->conn = conn;
	memcpy(&fecrr->chan_res, chan_res,
				sizeof(firefly_protocol_channel_response));
	struct firefly_event *ev = firefly_event_new(1,
			handle_channel_response_event, fecrr);

	ret = conn->event_queue->offer_event_cb(conn->event_queue, ev);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "could not add event to queue");
	}

}

int handle_channel_response_event(void *event_arg)
{
	struct firefly_event_chan_res_recv *fecrr =
		(struct firefly_event_chan_res_recv *) event_arg;
	int local_chan_id = fecrr->chan_res.dest_chan_id;
	struct firefly_channel *chan = find_channel_by_local_id(fecrr->conn,
			local_chan_id);

	firefly_protocol_channel_ack ack;
	ack.dest_chan_id = fecrr->chan_res.source_chan_id;
	if (chan != NULL) {
		// Received Channel request ack.
		chan->remote_id = fecrr->chan_res.source_chan_id;
		ack.source_chan_id = chan->local_id;
		ack.ack = true;
	} else {
		ack.source_chan_id = CHANNEL_ID_NOT_SET;
		ack.ack = false;
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1, "Received open"
						"channel on non-existing"
						"channel");
	}
	if (fecrr->chan_res.ack) {
		labcomm_encode_firefly_protocol_channel_ack(
						fecrr->conn->transport_encoder, &ack);
		// Should be done after encode above.
		fecrr->conn->on_channel_opened(chan);
	} else {
		chan->on_chan_rejected(fecrr->conn);
		firefly_channel_free(remove_channel_from_connection(chan,
									fecrr->conn));
	}
	free(event_arg);
	return 0;
}

void handle_channel_ack(firefly_protocol_channel_ack *chan_ack, void *context)
{
	int ret;
	struct firefly_connection *conn = (struct firefly_connection *) context;
	struct firefly_event_chan_ack_recv *fecar =
			malloc(sizeof(struct firefly_event_chan_ack_recv));
	if (fecar == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}
	fecar->conn = conn;
	memcpy(&fecar->chan_ack, chan_ack, sizeof(firefly_protocol_channel_ack));
	struct firefly_event *ev = firefly_event_new(1,
			handle_channel_ack_event, fecar);

	ret = conn->event_queue->offer_event_cb(conn->event_queue, ev);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "could not add event to queue");
	}
}

int handle_channel_ack_event(void *event_arg)
{
	struct firefly_event_chan_ack_recv *fecar =
		(struct firefly_event_chan_ack_recv *) event_arg;
	int local_chan_id = fecar->chan_ack.dest_chan_id;
	struct firefly_channel *chan = find_channel_by_local_id(fecar->conn,
			local_chan_id);
	if (chan != NULL) {
		fecar->conn->on_channel_opened(chan);
	} else {
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1, "Received ack"
						"on non-existing channel.\n");
	}
	free(event_arg);
	return 0;
}

void handle_channel_close(firefly_protocol_channel_close *chan_close,
		void *context)
{
	struct firefly_connection *conn = (struct firefly_connection *) context;
	struct firefly_channel *chan = find_channel_by_local_id(
			conn, chan_close->dest_chan_id);
	if (chan != NULL){
		create_channel_closed_event(chan);
	} else {
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1,
				"Received closed on non-existing channel.\n");
	}
}

void handle_data_sample(firefly_protocol_data_sample *data,
		void *context)
{
	struct firefly_connection *conn = (struct firefly_connection *) context;

	struct firefly_event_recv_sample *fers =
		malloc(sizeof(struct firefly_event_recv_sample));
	if (fers == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Could not allocate event.\n");
	}

	fers->conn = conn;
	memcpy(&fers->data, data, sizeof(firefly_protocol_data_sample));
	fers->data.app_enc_data.a = malloc(data->app_enc_data.n_0);
	memcpy(fers->data.app_enc_data.a, data->app_enc_data.a, data->app_enc_data.n_0);

	struct firefly_event *ev = firefly_event_new(1,
			handle_data_sample_event, fers);
	int ret = conn->event_queue->offer_event_cb(conn->event_queue, ev);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "could not add event to queue");
	}
}

int handle_data_sample_event(void *event_arg)
{
	struct firefly_event_recv_sample *fers =
		(struct firefly_event_recv_sample *) event_arg;
	struct firefly_channel *chan = find_channel_by_local_id(fers->conn,
			fers->data.dest_chan_id);

	if (chan != NULL) {
		chan->reader_data->data = fers->data.app_enc_data.a;
		chan->reader_data->data_size = fers->data.app_enc_data.n_0;
		chan->reader_data->pos = 0;
		labcomm_decoder_decode_one(chan->proto_decoder);
		chan->reader_data->data = NULL;
		chan->reader_data->data_size = 0;
		chan->reader_data->pos = 0;
	} else {
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1,
					  "Received data sample on non-"
					  "existing channel.\n");
	}
	free(fers->data.app_enc_data.a);
	free(event_arg);
	return 0;
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
