#include <protocol/firefly_protocol.h>
#include "protocol/firefly_protocol_private.h"

#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <labcomm.h>

#include <utils/firefly_errors.h>
#include <gen/firefly_protocol.h>
#include <utils/firefly_event_queue.h>

#include "utils/firefly_event_queue_private.h"
#include "utils/cppmacros.h"

/*
 * FIREFLY_PROTO_ACK_*: Used to use the existing ack-functionality
 * working with protocol types as well. This may or may not be the
 * right way to do this...
 *
 * Must be negative.
*/
#define FIREFLY_PROTO_ACK_RESTRICT_ACK -1


static void firefly_unknown_dest(struct firefly_connection *conn,
								 int src_id, int dest_id, const char *action)
{
	UNUSED_VAR(conn);
	firefly_protocol_channel_close chan_close;

	firefly_error(FIREFLY_ERROR_PROTO_STATE, 2,
				  "Received %s on a non-existent channel", action);

	chan_close.dest_chan_id = src_id;
	chan_close.source_chan_id = dest_id;

	labcomm_encode_firefly_protocol_channel_close(conn->transport_encoder,
						      &chan_close);
}

int firefly_channel_open_event(void *event_arg)
{
	struct firefly_connection        *conn;
	struct firefly_channel           *chan;
	firefly_protocol_channel_request chan_req;

	conn = event_arg;

	if (conn->open != FIREFLY_CONNECTION_OPEN) {
		firefly_channel_raise(NULL, conn, FIREFLY_ERROR_CONN_STATE,
					  "Can't open new channel on closed connection.\n");
		return -1;
	}

	chan = firefly_channel_new(conn);
	if (!chan) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "Could not allocate channel.\n");

		return -1;
	}

	add_channel_to_connection(chan, conn);

	chan_req.source_chan_id = chan->local_id;
	chan_req.dest_chan_id   = chan->remote_id;
	chan_req.auto_restrict  = false;

	labcomm_encoder_ioctl(conn->transport_encoder,
			FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID,
			&chan->important_id);

	labcomm_encode_firefly_protocol_channel_request(conn->transport_encoder,
							&chan_req);

	return 0;
}

void firefly_channel_open(struct firefly_connection *conn)
{
	int64_t ret;

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
						FIREFLY_PRIORITY_HIGH,
						firefly_channel_open_event,
						conn, 0, NULL);
	if (ret < 0)
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not add event.");
}

int firefly_channel_open_auto_restrict_event(void *event_arg)
{
	struct firefly_event_chan_open_auto_restrict *arg;
	struct firefly_channel_types types;
	struct firefly_connection        *conn;
	struct firefly_channel           *chan;
	firefly_protocol_channel_request chan_req;
	arg = event_arg;
	conn = arg->connection;
	types = arg->types;
	if (conn->open != FIREFLY_CONNECTION_OPEN) {
		firefly_channel_raise(NULL, conn, FIREFLY_ERROR_CONN_STATE,
			"Can't open new channel on closed connection.\n");
		return -1;
	}
	chan = firefly_channel_new(conn);
	if (!chan) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "Could not allocate channel.\n");
		return -1;
	}
	chan->auto_restrict = true;
	add_channel_to_connection(chan, conn);
	chan_req.source_chan_id = chan->local_id;
	chan_req.dest_chan_id   = chan->remote_id;
	chan_req.auto_restrict  = true;
        chan->types = types;
	labcomm_encoder_ioctl(conn->transport_encoder,
			      FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID,
			      &chan->important_id);
	labcomm_encode_firefly_protocol_channel_request(conn->transport_encoder,
							&chan_req);

	FIREFLY_FREE(event_arg);

	return 0;
}

void firefly_channel_open_auto_restrict( struct firefly_connection *conn,
					struct firefly_channel_types types)
{
	int64_t ret;
	struct firefly_event_chan_open_auto_restrict *ev;

	ev = FIREFLY_MALLOC(sizeof(*ev));
	if (ev) {
		ev->connection = conn;
		ev->types = types;
		ret = conn->event_queue->offer_event_cb(conn->event_queue,
				FIREFLY_PRIORITY_HIGH,
				firefly_channel_open_auto_restrict_event,
				ev, 0, NULL);
		if (ret < 0)
			firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not add event.");
	} else {
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not add event.");
	}
}

static int64_t create_channel_closed_event(struct firefly_channel *chan,
		unsigned int nbr_deps, const int64_t *deps)
{
	int64_t ret;

	ret = chan->conn->event_queue->offer_event_cb(chan->conn->event_queue,
			FIREFLY_PRIORITY_HIGH, firefly_channel_closed_event,
			chan, nbr_deps, deps);
	if (ret < 0)
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Could not add event.");
	return ret;
}

int firefly_channel_close_event(void *event_arg)
{
	struct firefly_connection       *conn;
	struct firefly_channel			*chan;
	firefly_protocol_channel_close chan_close;

	chan = event_arg;
	conn = chan->conn;

	chan->state = FIREFLY_CHANNEL_CLOSED;

	chan_close.dest_chan_id = chan->remote_id;
	chan_close.source_chan_id = chan->local_id;

	labcomm_encode_firefly_protocol_channel_close(conn->transport_encoder,
						      &chan_close);
	return 0;
}

int64_t firefly_channel_close(struct firefly_channel *chan)
{
	struct firefly_connection *conn;
	int64_t ret;

	conn = chan->conn;

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
						FIREFLY_PRIORITY_HIGH,
						firefly_channel_close_event,
						chan, 0, NULL);
	if (ret < 0) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "Could not add event to queue.");
	} else {
		ret = create_channel_closed_event(chan, 1, &ret);
	}

	return ret;
}

void protocol_data_received(struct firefly_connection *conn,
		unsigned char *data, size_t size)
{
	if (conn->open == FIREFLY_CONNECTION_OPEN) {
		labcomm_decoder_ioctl(conn->transport_decoder,
				FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER,
				data, size);
		int res = 0;
		while (res >= 0)
			res = labcomm_decoder_decode_one(conn->transport_decoder);
	}
}

void handle_channel_request(firefly_protocol_channel_request *chan_req,
		void *context)
{
	struct firefly_connection *conn;
	struct firefly_event_chan_req_recv *fecrr;
	int ret;

	conn = context;

	fecrr = FIREFLY_MALLOC(sizeof(*fecrr));
	if (fecrr == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "Could not allocate event.\n");
		return;
	}

	fecrr->conn = conn;
	memcpy(&fecrr->chan_req, chan_req, sizeof(*chan_req));

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
						FIREFLY_PRIORITY_HIGH,
						handle_channel_request_event,
						fecrr, 0, NULL);
	if (ret < 0) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "could not add event to queue");
		FIREFLY_FREE(fecrr);
	}
}

int handle_channel_request_event(void *event_arg)
{
	struct firefly_event_chan_req_recv *fecrr;
	struct firefly_channel *chan;
	struct firefly_connection *conn;
	int ret = 0;

	fecrr = event_arg;
	conn  = fecrr->conn;
	chan  = find_channel_by_remote_id(conn, fecrr->chan_req.source_chan_id);
	/**
	 * if chan != NULL the request was accepted but the response has probably
	 * been lost. The response is important and will be sent again.
	 */
	if (chan == NULL) {
		// Received Channel request.
		chan = firefly_channel_new(conn);
		if (chan == NULL) {
			firefly_error(FIREFLY_ERROR_ALLOC, 1,
				      "Could not allocate channel.\n");
			ret = -1;
		} else {
			firefly_protocol_channel_response res;

			chan->remote_id = fecrr->chan_req.source_chan_id;
			chan->auto_restrict = fecrr->chan_req.auto_restrict;
			add_channel_to_connection(chan, conn);

			res.dest_chan_id   = chan->remote_id;
			res.source_chan_id = chan->local_id;
			res.ack            = false;
			if (conn->actions != NULL && conn->actions->channel_recv != NULL)
				res.ack = conn->actions->channel_recv(chan);
			if (!res.ack) {
				res.source_chan_id = CHANNEL_ID_NOT_SET;
				firefly_channel_free(remove_channel_from_connection(chan, conn));
			} else {
				/* TODO: Decoder registrations. */
				labcomm_encoder_ioctl(fecrr->conn->transport_encoder,
						FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID,
						&chan->important_id);
			}
			labcomm_encode_firefly_protocol_channel_response(
					conn->transport_encoder, &res);
		}
	}

	FIREFLY_FREE(event_arg);

	return ret;
}

void handle_channel_response(firefly_protocol_channel_response *chan_res,
		void *context)
{
	struct firefly_connection *conn;
	struct firefly_event_chan_res_recv *fecrr;
	int ret;

	conn = context;

	fecrr = FIREFLY_MALLOC(sizeof(*fecrr));
	if (fecrr == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "Could not allocate event.\n");
		return;
	}
	fecrr->conn = conn;
	memcpy(&fecrr->chan_res, chan_res, sizeof(*chan_res));
	ret = conn->event_queue->offer_event_cb(conn->event_queue,
						FIREFLY_PRIORITY_HIGH,
						handle_channel_response_event,
						fecrr, 0, NULL);
	if (ret < 0) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "could not add event to queue");
		FIREFLY_FREE(fecrr);
	}
}

static void firefly_channel_send_channel_ack(
		struct firefly_connection *conn,
		struct firefly_channel *chan,
		int dest_chan_id)
{
	firefly_protocol_channel_ack ack;

	if (chan != NULL) {
		ack.ack = true;
		ack.source_chan_id = chan->local_id;
		ack.dest_chan_id = chan->remote_id;
	} else {
		ack.ack = false;
		ack.source_chan_id = CHANNEL_ID_NOT_SET;
		ack.dest_chan_id = dest_chan_id;
	}
	labcomm_encode_firefly_protocol_channel_ack(
				conn->transport_encoder, &ack);
}

int handle_channel_response_event(void *event_arg)
{
	struct firefly_event_chan_res_recv *fecrr;
	struct firefly_channel *chan;

	fecrr = event_arg;
	chan = find_channel_by_local_id(fecrr->conn,
					fecrr->chan_res.dest_chan_id);

	if (chan == NULL) {
		firefly_unknown_dest(fecrr->conn, fecrr->chan_res.source_chan_id,
                                     fecrr->chan_res.dest_chan_id, "channel_response");
	} else if (fecrr->chan_res.ack) {
		if (chan->remote_id == CHANNEL_ID_NOT_SET) {
			chan->remote_id = fecrr->chan_res.source_chan_id;
			firefly_channel_ack(chan);
			firefly_channel_internal_opened(chan);
			firefly_channel_set_types(chan, chan->types);
		}
		firefly_channel_send_channel_ack(fecrr->conn, chan,
				fecrr->chan_res.source_chan_id);
	} else if (chan != NULL) {
		firefly_channel_raise(chan, NULL, FIREFLY_ERROR_CHAN_REFUSED,
				"Channel was refused by remote end.");
		firefly_channel_ack(chan);
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

	conn = context;
	fecar = FIREFLY_MALLOC(sizeof(*fecar));
	if (fecar == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "Could not allocate event.\n");
		return;
	}

	fecar->conn = conn;
	memcpy(&fecar->chan_ack, chan_ack, sizeof(*chan_ack));

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
			FIREFLY_PRIORITY_HIGH, handle_channel_ack_event, fecar, 0, NULL);
	if (ret < 0) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "could not add event to queue");
		FIREFLY_FREE(fecar);
	}
}

int handle_channel_ack_event(void *event_arg)
{
	struct firefly_event_chan_ack_recv *fecar;
	struct firefly_channel *chan;

	fecar = event_arg;
	chan = find_channel_by_local_id(fecar->conn,
					fecar->chan_ack.dest_chan_id);
	if (chan != NULL) {
		firefly_channel_ack(chan);
		firefly_channel_internal_opened(chan); /* TODO: Why? */
	} else {
		firefly_unknown_dest(fecar->conn, fecar->chan_ack.source_chan_id,
							 fecar->chan_ack.dest_chan_id, "channel_ack");
	}
	FIREFLY_FREE(event_arg);

	return 0;
}

void handle_channel_close(firefly_protocol_channel_close *chan_close,
		void *context)
{
	struct firefly_connection *conn;
	struct firefly_channel *chan;

	conn = context;
	chan = find_channel_by_local_id(conn, chan_close->dest_chan_id);
	if (chan != NULL){
		chan->state = FIREFLY_CHANNEL_CLOSED;
		create_channel_closed_event(chan, 0, NULL);
	} else {
		// TODO silently discard?
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

	conn = context;
	fers = FIREFLY_RUNTIME_MALLOC(conn, sizeof(*fers));
	fers_data = FIREFLY_RUNTIME_MALLOC(conn, data->app_enc_data.n_0);
	if (fers == NULL || fers_data == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "Could not allocate event.\n");
		FIREFLY_RUNTIME_FREE(conn, fers_data);
		FIREFLY_RUNTIME_FREE(conn, fers);
		return;
	}

	fers->conn = conn;
	memcpy(&fers->data, data, sizeof(*data));
	memcpy(fers_data, data->app_enc_data.a, data->app_enc_data.n_0);
	fers->data.app_enc_data.a = fers_data;

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
			FIREFLY_PRIORITY_LOW, handle_data_sample_event, fers, 0, NULL);
	if (ret < 0) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "could not add event to queue");
		FIREFLY_RUNTIME_FREE(conn, fers->data.app_enc_data.a);
		FIREFLY_RUNTIME_FREE(conn, fers);
	}
}

int handle_data_sample_event(void *event_arg)
{
	struct firefly_event_recv_sample *fers;
	struct firefly_channel *chan;

	fers = event_arg;

	chan = find_channel_by_local_id(fers->conn, fers->data.dest_chan_id);

	if (chan != NULL) {
                /* FIXME: Undefined wrapping behaviour. */
		int expected_seqno = chan->remote_seqno + 1;

		if (expected_seqno <= 0) {
			expected_seqno = 1;
		}
		if (fers->data.important) {
			firefly_protocol_ack ack_pkt;

			ack_pkt.dest_chan_id = chan->remote_id;
			ack_pkt.src_chan_id = chan->local_id;
			ack_pkt.seqno = fers->data.seqno;
			labcomm_encode_firefly_protocol_ack(
					chan->conn->transport_encoder,
					&ack_pkt);
		}
		if (!fers->data.important ||
		    expected_seqno == fers->data.seqno)
		{
			size_t size;
			int id;

			size = fers->data.app_enc_data.n_0;
			labcomm_decoder_ioctl(chan->proto_decoder,
					FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER,
					fers->data.app_enc_data.a,
					size);

			id = labcomm_decoder_decode_one(chan->proto_decoder);
			if (fers->data.important) {
				chan->remote_seqno = fers->data.seqno;
			}
			if (id == -ENOENT) {
#if 0
				if (!chan->auto_restrict) {
					firefly_error(FIREFLY_ERROR_LABCOMM, 1,
						      "Unkn. type. Use autorestr.");
				} else {
					firefly_error(FIREFLY_ERROR_LABCOMM, 1,
						      "Wait for restr.");
				}
#endif
			} else if (!chan->restricted_local &&
				   chan->auto_restrict)
			{
				size_t n = 0;

				for (; n < chan->n_decoder_types; n++) {
					if (chan->seen_decoder_ids[n] == -1 ||
					    chan->seen_decoder_ids[n] == id)
					{
						break;
					}
				}
				chan->seen_decoder_ids[n] = id;
				if (n == chan->n_decoder_types-1) {
					FIREFLY_FREE(chan->seen_decoder_ids);
					chan->seen_decoder_ids = NULL;
					chan->n_decoder_types = 0; /* State-ish */
					channel_auto_restr_send_ack(chan);
				}
			}
		} else if (fers->data.important &&
			   expected_seqno != fers->data.seqno)
		{
#if 0	/* This would probably be a good idea, but it breaks existing tests. */
			firefly_error(FIREFLY_ERROR_PROTO_STATE, 1,
				      "Received data flagged important with "
				      "unexpected sequence number.");
#endif
		}
	} else {
		firefly_unknown_dest(fers->conn, fers->data.src_chan_id,
							 fers->data.dest_chan_id, "data_sample");
	}

	FIREFLY_RUNTIME_FREE(fers->conn, fers->data.app_enc_data.a);
	FIREFLY_RUNTIME_FREE(fers->conn, event_arg);

	return 0;
}

void handle_ack(firefly_protocol_ack *ack, void *context)
{
	struct firefly_connection *conn;
	struct firefly_channel *chan;

	conn = context;
	chan = find_channel_by_local_id(conn, ack->dest_chan_id);
	if (chan == NULL) {
		firefly_unknown_dest(conn, ack->src_chan_id, ack->dest_chan_id, "ack");
	} else if ((chan->current_seqno == ack->seqno && ack->seqno > 0) ||
		   (chan->auto_restrict && chan->restricted_local &&
		    ack->seqno == FIREFLY_PROTO_ACK_RESTRICT_ACK))
	{
		firefly_channel_ack(chan);
	}
}

struct firefly_channel *find_channel_by_remote_id(
		struct firefly_connection *conn, int id)
{
	struct channel_list_node *head;

	head = conn->chan_list;
	while (head) {
		if (head->chan->remote_id == id)
			break;
		head = head->next;
	}
	return (head) ? head->chan : NULL;
}

struct firefly_channel *find_channel_by_local_id(
		struct firefly_connection *conn, int id)
{
	struct channel_list_node *head;

	head = conn->chan_list;
	while (head) {
		if (head->chan->local_id == id)
			break;
		head = head->next;
	}
	return (head) ? head->chan : NULL;
}

void add_channel_to_connection(struct firefly_channel *chan,
		struct firefly_connection *conn)
{
	struct channel_list_node *new_node;

	new_node = FIREFLY_MALLOC(sizeof(*new_node));

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

void handle_channel_restrict_request(
		firefly_protocol_channel_restrict_request *data,
		void *context)
{
	struct firefly_connection *conn;
	struct firefly_event_channel_restrict_request *earg;
	int ret;

	conn = context;
	earg = FIREFLY_MALLOC(sizeof(*earg));
	if (!earg) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "Could not allocate event arg.\n");
		return;
	}
	memcpy(&earg->rreq, data, sizeof(*data));
	earg->conn = conn;
	ret = conn->event_queue->offer_event_cb(conn->event_queue,
						FIREFLY_PRIORITY_MEDIUM,
						&channel_restrict_request_event,
						earg, 0, NULL);
	if (ret < 0) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "Could not add event to queue");
		FIREFLY_FREE(earg);
	}
}

int channel_restrict_request_event(void *context)
{
	struct firefly_event_channel_restrict_request *earg;
	struct firefly_connection *conn;
	struct firefly_channel *chan;
	firefly_protocol_channel_restrict_ack resp;

	earg = context;
	conn = earg->conn;
	chan = find_channel_by_local_id(conn, earg->rreq.dest_chan_id);
	if (!chan) {
		firefly_unknown_dest(conn, earg->rreq.source_chan_id,
							 earg->rreq.dest_chan_id, "channel_restrict_request");
		FIREFLY_FREE(earg);
		return -1;
	}
	resp.dest_chan_id   = chan->remote_id;
	resp.source_chan_id = chan->local_id;

	chan->restricted_remote = earg->rreq.restricted;
	if (chan->restricted_remote) {
		if (!chan->restricted_local) {
			/* Remotely initiated restrict. */
			if (conn->actions && conn->actions->channel_restrict)
				chan->restricted_local =
					conn->actions->channel_restrict(chan);
			/* Otherwise deny incoming requests. */

			/*
			 * Here it might be appropriate to call the info
			 * callback too, despite it beeing redundant...
			 */
		}
	} else {
		if (chan->restricted_local) {
			/* Remotely initiated unrestrict. */
			if (conn->actions && conn->actions->channel_restrict_info)
				conn->actions->channel_restrict_info(chan, UNRESTRICTED);
			chan->restricted_local = false;
		}
	}
	resp.restricted = chan->restricted_local;

	labcomm_encode_firefly_protocol_channel_restrict_ack(
			conn->transport_encoder,
			&resp);
	FIREFLY_FREE(earg);

	return 0;
}

void handle_channel_restrict_ack(firefly_protocol_channel_restrict_ack *data,
				 void *context)
{
	struct firefly_connection *conn;
	struct firefly_event_chan_restrict_ack *earg;
	int ret;

	conn = context;

	earg = FIREFLY_MALLOC(sizeof(*earg));
	if (!earg) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "Could not allocate event arg.\n");
		return;
	}
	memcpy(&earg->rack, data, sizeof(*data));
	earg->conn = conn;
	ret = conn->event_queue->offer_event_cb(conn->event_queue,
						FIREFLY_PRIORITY_MEDIUM,
						channel_restrict_ack_event,
						earg, 0, NULL);
	if (ret < 0) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "Could not add event to queue");
		FIREFLY_FREE(earg);
	}
}

int channel_restrict_ack_event(void *context)
{
	struct firefly_event_chan_restrict_ack *earg;
	struct firefly_connection *conn;
	struct firefly_channel *chan;

	earg = context;
	conn = earg->conn;
	chan = find_channel_by_local_id(conn, earg->rack.dest_chan_id);
	if (!chan) {
		firefly_unknown_dest(conn, earg->rack.source_chan_id,
							 earg->rack.dest_chan_id, "restrict_ack");
		FIREFLY_FREE(context);
		return -1;
	}
	if (chan->auto_restrict) {
		firefly_protocol_ack ack_pkt;

		/* TODO: Make sure user is not notified ico duplicate. */
		chan->restricted_remote = true; /* TODO: Use member. */
		channel_auto_restr_check_complete(chan);

		ack_pkt.dest_chan_id = chan->remote_id;
		ack_pkt.src_chan_id = chan->local_id;
		ack_pkt.seqno = FIREFLY_PROTO_ACK_RESTRICT_ACK;
		labcomm_encode_firefly_protocol_ack(
			chan->conn->transport_encoder,
			&ack_pkt);
	} else {
		if (earg->rack.restricted) {
			if (!chan->restricted_remote && chan->restricted_local &&
			    chan->conn->actions &&
			    chan->conn->actions->channel_restrict_info)
			{
				conn->actions->channel_restrict_info(chan,
								     RESTRICTED);
			} else if (!chan->restricted_local) {
				firefly_channel_raise(chan, NULL,
					FIREFLY_ERROR_PROTO_STATE,
					"Inconsistent restrict state.");
			}
		} else {
			enum restriction_transition t;

			t = chan->restricted_local ?
				RESTRICTION_DENIED : UNRESTRICTED;
			if (conn->actions && conn->actions->channel_restrict_info)
				conn->actions->channel_restrict_info(chan, t);
			chan->restricted_local = false;
		}
	}
	chan->restricted_remote = earg->rack.restricted;
	firefly_channel_ack(chan);
	FIREFLY_FREE(earg);

	return 0;
}

#if 0
struct firefly_channel_types *firefly_channel_types_new(void)
{
	struct firefly_channel_types *ct;

	ct = FIREFLY_MALLOC(sizeof(*ct));
	if (ct) {
		ct->decoder_types = NULL;
		ct->encoder_types = NULL;
	}
	return ct;
}

void firefly_channel_types_free(struct firefly_channel_types *ct)
{
	FIREFLY_FREE(ct);
}
#endif

void firefly_channel_types_add_decoder_type(
	struct firefly_channel_types *types,
	firefly_labcomm_decoder_register_function register_func,
	firefly_labcomm_handler_function handler,
	void *context)
{
	struct firefly_channel_decoder_type *dt;

	dt = FIREFLY_MALLOC(sizeof(*dt));
	if (dt) {
		dt->register_func = register_func;
		dt->handler = handler;
		dt->context = context;
		dt->next = types->decoder_types;
		types->decoder_types = dt;
	}
	/* TODO: Error handl. */
}

void firefly_channel_types_add_encoder_type(
	struct firefly_channel_types *types,
	firefly_labcomm_encoder_register_function register_func)
{
	struct firefly_channel_encoder_type *et;

	et = FIREFLY_MALLOC(sizeof(*et));
	if (et) {
		et->register_func = register_func;
		et->next = types->encoder_types;
		types->encoder_types = et;
	}
	/* TODO: Error handl. */
}

void firefly_channel_set_types(struct firefly_channel *chan,
			       struct firefly_channel_types types)
{
        chan->types = types;
}
