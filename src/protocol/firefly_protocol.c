#include <protocol/firefly_protocol.h>
#include "protocol/firefly_protocol_private.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>

#include <labcomm.h>

#include <utils/firefly_errors.h>
#include <gen/firefly_protocol.h>
#include <utils/firefly_event_queue.h>

#include "utils/firefly_event_queue_private.h"
#include "utils/cppmacros.h"

static void signature_trans_write(unsigned char *data, size_t size,
			struct firefly_connection *conn, bool important, unsigned char *id)
{
	UNUSED_VAR(important);
	UNUSED_VAR(id);
	protocol_data_received(conn, data, size);
}

void reg_proto_sigs(struct labcomm_encoder *enc,
					struct labcomm_decoder *dec,
					struct firefly_connection *conn)
{
	transport_write_f orig_twf = conn->transport_write;
	conn->transport_write = signature_trans_write;

	labcomm_decoder_register_firefly_protocol_data_sample(dec,
					  	  handle_data_sample, conn);

	labcomm_decoder_register_firefly_protocol_channel_request(dec,
						  handle_channel_request, conn);

	labcomm_decoder_register_firefly_protocol_channel_response(dec,
					   handle_channel_response, conn);

	labcomm_decoder_register_firefly_protocol_channel_ack(dec,
						  handle_channel_ack, conn);

	labcomm_decoder_register_firefly_protocol_channel_close(dec,
						handle_channel_close, conn);

	labcomm_decoder_register_firefly_protocol_ack(dec,
						handle_ack, conn);

	labcomm_encoder_register_firefly_protocol_data_sample(enc);
	labcomm_encoder_register_firefly_protocol_channel_request(enc);
	labcomm_encoder_register_firefly_protocol_channel_response(enc);
	labcomm_encoder_register_firefly_protocol_channel_ack(enc);
	labcomm_encoder_register_firefly_protocol_channel_close(enc);
	labcomm_encoder_register_firefly_protocol_ack(enc);

	conn->transport_write = orig_twf;
}

void firefly_channel_open(struct firefly_connection *conn,
		firefly_channel_rejected_f on_chan_rejected)
{
	struct firefly_event_chan_open *feco;
	int ret;

	if (conn->open != FIREFLY_CONNECTION_OPEN) {
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1,
					  "Can't open new channel on closed connection.\n");
		return;
	}

	feco = FIREFLY_MALLOC(sizeof(struct firefly_event_chan_open));
	if (feco == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not allocate event.\n");
		FIREFLY_FREE(feco);
		return;
	}

	feco->conn        = conn;
	feco->rejected_cb = on_chan_rejected;

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
			FIREFLY_PRIORITY_HIGH, firefly_channel_open_event, feco);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not add event to queue");
		FIREFLY_FREE(feco);
	}
}

int firefly_channel_open_event(void *event_arg)
{
	struct firefly_event_chan_open   *feco;
	struct firefly_connection        *conn;
	struct firefly_channel           *chan;
	firefly_protocol_channel_request chan_req;

	feco = (struct firefly_event_chan_open *) event_arg;
	conn = feco->conn;

	chan = firefly_channel_new(conn);
	if (chan == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not allocate channel.\n");
		FIREFLY_FREE(event_arg);

		return -1;
	}

	chan->on_chan_rejected = feco->rejected_cb;

	add_channel_to_connection(chan, conn);

	chan_req.source_chan_id = chan->local_id;
	chan_req.dest_chan_id   = chan->remote_id;

	labcomm_encoder_ioctl(conn->transport_encoder,
			FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID,
			&chan->important_id);
	labcomm_encode_firefly_protocol_channel_request(conn->transport_encoder,
													&chan_req);
	FIREFLY_FREE(event_arg);
	return 0;
}

static void create_channel_closed_event(struct firefly_channel *chan)
{
	int ret = chan->conn->event_queue->offer_event_cb(chan->conn->event_queue,
							FIREFLY_PRIORITY_HIGH, firefly_channel_closed_event,
							chan);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not add event to queue.");
	}
}

void firefly_channel_close(struct firefly_channel *chan)
{
	struct firefly_event_chan_close *fecc;
	struct firefly_connection *conn;
	int ret;

	fecc = FIREFLY_MALLOC(sizeof(struct firefly_event_chan_close));
	if (fecc == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not create event.");
		FIREFLY_FREE(fecc);
		return;
	}

	conn = chan->conn;

	fecc->conn = conn;
	fecc->chan_close.dest_chan_id = chan->remote_id;
	fecc->chan_close.source_chan_id = chan->local_id;
	ret = conn->event_queue->offer_event_cb(
						conn->event_queue, FIREFLY_PRIORITY_HIGH,
						firefly_channel_close_event, fecc);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not add event to queue.");
		FIREFLY_FREE(fecc);
		return; // We don't want to call the function below if we failed here
	}

	create_channel_closed_event(chan);
}

int firefly_channel_close_event(void *event_arg)
{
	struct firefly_event_chan_close *fecc;
	struct firefly_connection       *conn;

	fecc = (struct firefly_event_chan_close *) event_arg;
	conn = fecc->conn;

	labcomm_encode_firefly_protocol_channel_close(conn->transport_encoder,
												  &fecc->chan_close);
	FIREFLY_FREE(event_arg);

	return 0;
}

// TODO: Add const to data?
void protocol_data_received(struct firefly_connection *conn,
		unsigned char *data, size_t size)
{
	if (conn->open == FIREFLY_CONNECTION_OPEN) {
		labcomm_decoder_ioctl(conn->transport_decoder,
				FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER,
				data, size);
		labcomm_decoder_decode_one(conn->transport_decoder);
	}
}

void handle_channel_request(firefly_protocol_channel_request *chan_req,
		void *context)
{
	struct firefly_connection *conn;
	struct firefly_event_chan_req_recv *fecrr;
	int ret;

	conn = (struct firefly_connection *) context;

	fecrr = FIREFLY_MALLOC(sizeof(struct firefly_event_chan_req_recv));
	if (fecrr == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not allocate event.\n");
		FIREFLY_FREE(fecrr);
		return;
	}

	fecrr->conn = conn;
	memcpy(&fecrr->chan_req, chan_req,
				sizeof(firefly_protocol_channel_request));

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
			FIREFLY_PRIORITY_HIGH, handle_channel_request_event, fecrr);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "could not add event to queue");
		FIREFLY_FREE(fecrr);
	}
}

int handle_channel_request_event(void *event_arg)
{
	struct firefly_event_chan_req_recv *fecrr;
	struct firefly_channel *chan;
	struct firefly_connection *conn;
	int res = 0;

	fecrr = (struct firefly_event_chan_req_recv *) event_arg;
	conn  = fecrr->conn;

	chan  = find_channel_by_local_id(conn, fecrr->chan_req.dest_chan_id);
	if (chan != NULL) {
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1, "Received open"
					  "channel on existing channel.\n");
		res = -1;
	} else {
		// Received Channel request.
		chan = firefly_channel_new(conn);
		if (chan == NULL) {
			firefly_error(FIREFLY_ERROR_ALLOC, 1,
						  "Could not allocate channel.\n");
			res = -1;
		} else {
			firefly_protocol_channel_response res;

			chan->remote_id = fecrr->chan_req.source_chan_id;

			add_channel_to_connection(chan, conn);

			res.dest_chan_id   = chan->remote_id;
			res.source_chan_id = chan->local_id;
			res.ack            = conn->on_channel_recv(chan);
			if (!res.ack) {
				res.source_chan_id = CHANNEL_ID_NOT_SET;
				firefly_channel_free(remove_channel_from_connection(chan, conn));
			} else {
				labcomm_encoder_ioctl(fecrr->conn->transport_encoder,
						FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID,
						&chan->important_id);
			}
			labcomm_encode_firefly_protocol_channel_response(
					conn->transport_encoder, &res);
		}
	}

	FIREFLY_FREE(event_arg);

	return res;
}

void handle_channel_response(firefly_protocol_channel_response *chan_res,
		void *context)
{
	struct firefly_connection *conn;
	struct firefly_event_chan_res_recv *fecrr;
	int ret;

	conn = (struct firefly_connection *) context;

	fecrr = FIREFLY_MALLOC(sizeof(struct firefly_event_chan_res_recv));
	if (fecrr == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not allocate event.\n");
		FIREFLY_FREE(fecrr);
		return;
	}

	fecrr->conn = conn;
	memcpy(&fecrr->chan_res, chan_res,
		   sizeof(firefly_protocol_channel_response));

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
			FIREFLY_PRIORITY_HIGH, handle_channel_response_event, fecrr);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "could not add event to queue");
		FIREFLY_FREE(fecrr);
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
		if (chan != NULL) {
			fecrr->conn->transport_ack(chan->important_id, fecrr->conn);
			chan->important_id = 0;
			fecrr->conn->on_channel_opened(chan);
		}
	} else {
		chan->on_chan_rejected(fecrr->conn);
		firefly_channel_free(remove_channel_from_connection(chan,
								fecrr->conn));
	}
	FIREFLY_FREE(event_arg);

	return 0;
}

void handle_channel_ack(firefly_protocol_channel_ack *chan_ack, void *context)
{
	struct firefly_connection *conn;
	struct firefly_event_chan_ack_recv *fecar;
	int ret;

	conn = (struct firefly_connection *) context;

	fecar = FIREFLY_MALLOC(sizeof(struct firefly_event_chan_ack_recv));
	if (fecar == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not allocate event.\n");
		FIREFLY_FREE(fecar);
		return;
	}

	fecar->conn = conn;
	memcpy(&fecar->chan_ack, chan_ack, sizeof(firefly_protocol_channel_ack));

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
			FIREFLY_PRIORITY_HIGH, handle_channel_ack_event, fecar);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "could not add event to queue");
		FIREFLY_FREE(fecar);
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
		fecar->conn->transport_ack(chan->important_id, fecar->conn);
		chan->important_id = 0;
		fecar->conn->on_channel_opened(chan);
	} else {
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1, "Received ack"
						"on non-existing channel.\n");
	}
	FIREFLY_FREE(event_arg);

	return 0;
}

void handle_channel_close(firefly_protocol_channel_close *chan_close,
		void *context)
{
	struct firefly_connection *conn;
	struct firefly_channel *chan;

	conn = (struct firefly_connection *) context;
	chan = find_channel_by_local_id(conn, chan_close->dest_chan_id);
	if (chan != NULL){
		create_channel_closed_event(chan);
	} else {
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1,
					  "Received closed on non-existing channel.\n");
	}
}

void handle_data_sample(firefly_protocol_data_sample *data, void *context)
{
	struct firefly_connection *conn;
	struct firefly_event_recv_sample *fers;
	unsigned char *fers_data;
	int ret;

	conn = (struct firefly_connection *) context;

	fers      = FIREFLY_RUNTIME_MALLOC(conn,
			sizeof(struct firefly_event_recv_sample));
	fers_data = FIREFLY_RUNTIME_MALLOC(conn, data->app_enc_data.n_0);
	if (fers == NULL || fers_data == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not allocate event.\n");
		FIREFLY_RUNTIME_FREE(conn, fers_data);
		FIREFLY_RUNTIME_FREE(conn, fers);
		return;
	}

	fers->conn = conn;

	memcpy(&fers->data, data, sizeof(firefly_protocol_data_sample));

	memcpy(fers_data, data->app_enc_data.a, data->app_enc_data.n_0);
	fers->data.app_enc_data.a = fers_data;

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
			FIREFLY_PRIORITY_LOW, handle_data_sample_event, fers);
	if (ret) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "could not add event to queue");
		FIREFLY_RUNTIME_FREE(conn, fers->data.app_enc_data.a);
		FIREFLY_RUNTIME_FREE(conn, fers);
	}
}

int handle_data_sample_event(void *event_arg)
{
	struct firefly_event_recv_sample *fers =
		(struct firefly_event_recv_sample *) event_arg;
	struct firefly_channel *chan = find_channel_by_local_id(fers->conn,
			fers->data.dest_chan_id);

	if (chan != NULL) {
		int expected_seqno = chan->remote_seqno + 1;
		if (expected_seqno <= 0) {
			expected_seqno = 1;
		}
		if (!fers->data.important || expected_seqno == fers->data.seqno) {

			size_t size = 0;
			size = fers->data.app_enc_data.n_0;
			labcomm_decoder_ioctl(chan->proto_decoder,
					FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER,
					fers->data.app_enc_data.a,
					size);

			labcomm_decoder_decode_one(chan->proto_decoder);
			if (fers->data.important) {
				chan->remote_seqno = fers->data.seqno;
			}
		}
		if (fers->data.important) {
			firefly_protocol_ack ack_pkt;
			ack_pkt.dest_chan_id = chan->local_id;
			ack_pkt.src_chan_id = chan->remote_id;
			ack_pkt.seqno = fers->data.seqno;
			labcomm_encode_firefly_protocol_ack(chan->conn->transport_encoder,
					&ack_pkt);
		}
	} else {
		firefly_protocol_channel_close chan_close;

		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1,
					  "Received data sample on non-existing channel.\n");

		/* Notify the other party of their mistake. */
		chan_close.dest_chan_id   = fers->data.src_chan_id;
		chan_close.source_chan_id = fers->data.dest_chan_id;
		labcomm_encode_firefly_protocol_channel_close(fers->conn->transport_encoder,
													  &chan_close);
	}

	FIREFLY_RUNTIME_FREE(fers->conn, fers->data.app_enc_data.a);
	FIREFLY_RUNTIME_FREE(fers->conn, event_arg);

	return 0;
}

void handle_ack(firefly_protocol_ack *ack, void *context)
{
	struct firefly_connection *conn = (struct firefly_connection *) context;
	struct firefly_channel *chan =
		find_channel_by_local_id(conn, ack->dest_chan_id);
	if (chan != NULL && chan->current_seqno == ack->seqno && ack->seqno > 0) {
		conn->transport_ack(chan->important_id, conn);
		chan->important_id = 0;
		if (chan->important_queue != NULL) {
			// If there are queued important packets, send the next one
			struct firefly_event_send_sample *fess;
			fess = chan->important_queue->fess;
			struct firefly_channel_important_queue *tmp = chan->important_queue;
			chan->important_queue = tmp->next;
			FIREFLY_FREE(tmp);
			conn->event_queue->offer_event_cb(conn->event_queue,
					FIREFLY_PRIORITY_HIGH, send_data_sample_event, fess);
		}
	} else if (chan->current_seqno > ack->seqno) {
		// Do nothing, old ack
	} else {
		// TODO errornous packet
	}
}

struct firefly_channel *find_channel_by_local_id(
		struct firefly_connection *conn, int id)
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
	struct channel_list_node *new_node;

	new_node = FIREFLY_MALLOC(sizeof(struct channel_list_node));

	if (new_node != NULL) {
		new_node->chan = chan;
		new_node->next = conn->chan_list;
		conn->chan_list = new_node;
	} else {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
					  "Failed to allocate new channel list note\n");
	}
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
	size_t cnt = 0;

	node = conn->chan_list;
	while (node != NULL) {
		node = node->next;
		cnt++;
	}

	return cnt;
}
