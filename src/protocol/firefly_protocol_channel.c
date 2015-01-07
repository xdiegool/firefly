#include "protocol/firefly_protocol_private.h"

#include <limits.h>

#include <utils/firefly_errors.h>
#include "utils/firefly_event_queue_private.h"

struct firefly_channel *firefly_channel_new(struct firefly_connection *conn)
{
	struct firefly_channel *chan;

	chan = FIREFLY_MALLOC(sizeof(*chan));
        if (!chan) {
		FFL(FIREFLY_ERROR_ALLOC);
		return NULL;
        }
	chan->conn              = conn;
	chan->local_id		= next_channel_id(conn);
	chan->remote_id		= CHANNEL_ID_NOT_SET;
	chan->state		= FIREFLY_CHANNEL_READY;
	chan->important_queue	= NULL;
	chan->important_id	= 0;
	chan->current_seqno	= 0;
	chan->remote_seqno	= 0;
	chan->restricted_local	= false;
	chan->restricted_remote	= false;
	chan->auto_restrict	= false;
	chan->enc_types		= NULL;
	chan->seen_decoder_ids 	= NULL;
	chan->n_decoder_types	= 0;
	chan->proto_decoder     = NULL;
	chan->proto_encoder     = NULL;

	// TODO: Fix this once Labcomm re-gets error handling
	/* labcomm_register_error_handler_encoder(proto_encoder,*/
	/*                 labcomm_error_to_ff_error);*/
	/* labcomm_register_error_handler_decoder(proto_decoder,*/
	/*                 labcomm_error_to_ff_error);*/

	return chan;
}

void firefly_channel_free(struct firefly_channel *chan)
{
	struct firefly_channel_important_queue *node;
	struct firefly_channel_important_queue *tmp;
	if (!chan)
		return;
	if (chan->proto_decoder)
		labcomm_decoder_free(chan->proto_decoder);
	if (chan->proto_encoder)
		labcomm_encoder_free(chan->proto_encoder);
	node = chan->important_queue;
	while (node != NULL) {
		tmp = node;
		node = node->next;
		FIREFLY_FREE(tmp->event_arg);
		FIREFLY_FREE(tmp);
	}
	while (chan->enc_types) {
		struct firefly_channel_encoder_type *tmp;

		tmp = chan->enc_types;
		chan->enc_types = tmp->next;
		FIREFLY_FREE(tmp);
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
	if (chan->current_seqno == INT_MAX) {
		chan->current_seqno = 0;
	}
	return ++chan->current_seqno;
}

int firefly_channel_closed_event(void *event_arg)
{
	struct firefly_channel *chan;

	chan = event_arg;

	firefly_channel_ack(chan);

	remove_channel_from_connection(chan, chan->conn);
	if (chan->conn->actions && chan->conn->actions->channel_closed)
		chan->conn->actions->channel_closed(chan);
	firefly_channel_free(chan);

	return 0;
}

void firefly_channel_restrict(struct firefly_channel *chan)
{
	struct firefly_connection *conn;
	int ret;

	conn = chan->conn;
	ret = conn->event_queue->offer_event_cb(conn->event_queue,
						FIREFLY_PRIORITY_MEDIUM,
						firefly_channel_restrict_event,
						chan, 0, NULL);
	if (ret < 0)
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "could not add event to queue");
}

int firefly_channel_restrict_event(void *earg)
{
	struct firefly_channel *chan;
	firefly_protocol_channel_restrict_request req;
	struct labcomm_encoder *tenc;

	chan = earg;
	if (chan->restricted_local)
		return -1; /* Already [in the process of beeing] restricted */
	if (chan->restricted_remote)
		return 0;  /* In process of answering remote request. */
	if (!firefly_channel_enqueue_important(chan,
				firefly_channel_restrict_event, earg)) {
		chan->restricted_local = true;

		req.dest_chan_id   = chan->remote_id;
		req.source_chan_id = chan->local_id;
		req.restricted     = true;

		tenc = chan->conn->transport_encoder;
		labcomm_encoder_ioctl(tenc,
					  FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID,
					  &chan->important_id);
		labcomm_encode_firefly_protocol_channel_restrict_request(tenc, &req);
	}

	return 0;
}

void firefly_channel_unrestrict(struct firefly_channel *chan)
{
	struct firefly_connection *conn;
	int ret;

	conn = chan->conn;
	ret = conn->event_queue->offer_event_cb(conn->event_queue,
						FIREFLY_PRIORITY_MEDIUM,
						firefly_channel_unrestrict_event,
						chan, 0, NULL);
	if (ret < 0)
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "could not add event to queue");
}

int firefly_channel_unrestrict_event(void *earg)
{
	struct firefly_channel *chan;
	firefly_protocol_channel_restrict_request req;
	struct labcomm_encoder *tenc;

	chan = earg;
	if (!chan->restricted_local)
		return -1; /* Tried to unrestrict unrestricted channel. */
	if (!chan->restricted_remote)
		return 0;  /* Previous request not completed.  */

	if (!firefly_channel_enqueue_important(chan,
				firefly_channel_restrict_event, earg)) {
		chan->restricted_local = false;

		req.dest_chan_id   = chan->remote_id;
		req.source_chan_id = chan->local_id;
		req.restricted     = false;

		tenc = chan->conn->transport_encoder;
		labcomm_encoder_ioctl(tenc,
					  FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID,
					  &chan->important_id);
		labcomm_encode_firefly_protocol_channel_restrict_request(tenc, &req);
	}

	return 0;
}

void firefly_channel_internal_opened(struct firefly_channel *chan)
{
	struct labcomm_decoder *proto_decoder;
	struct labcomm_encoder *proto_encoder;
	struct labcomm_reader  *reader;
	struct labcomm_writer  *writer;
	struct firefly_connection *conn;
	struct firefly_channel_types types;

	if (chan->state == FIREFLY_CHANNEL_OPEN)
		return;

	conn = chan->conn;
	chan->state = FIREFLY_CHANNEL_OPEN;

	reader = protocol_labcomm_reader_new(conn, conn->lc_memory);
	writer = protocol_labcomm_writer_new(chan, conn->lc_memory);
	if (!reader || !writer) {
		FFL(FIREFLY_ERROR_ALLOC);
		protocol_labcomm_reader_free(reader);
		protocol_labcomm_writer_free(writer);
		FIREFLY_FREE(chan);
		return;         /* FIXME: call ff_err()... */
	}
	proto_decoder = labcomm_decoder_new(reader, NULL, conn->lc_memory, NULL);
	proto_encoder = labcomm_encoder_new(writer, NULL, conn->lc_memory, NULL);
	if (!proto_decoder || !proto_encoder) {
		FFL(FIREFLY_ERROR_ALLOC);
		if (chan->proto_decoder)
			labcomm_decoder_free(chan->proto_decoder);
		if (chan->proto_encoder)
			labcomm_encoder_free(chan->proto_encoder);
		protocol_labcomm_reader_free(reader);
		protocol_labcomm_writer_free(writer);
		FIREFLY_FREE(chan);
		return;         /* FIXME: call ff_err()... */
	}
	chan->proto_decoder	= proto_decoder;
	chan->proto_encoder	= proto_encoder;


	chan->conn->actions->channel_opened(chan);

        /* Reg */
        types = chan->types;
        while (types.decoder_types) {
		struct firefly_channel_decoder_type *f;

		f = types.decoder_types;
		f->register_func(chan->proto_decoder, f->handler, f->context);
		types.decoder_types = f->next;
		FIREFLY_FREE(f);
		chan->n_decoder_types++;
	}
	chan->seen_decoder_ids = calloc(chan->n_decoder_types,
					sizeof(*chan->seen_decoder_ids));
	if (chan->seen_decoder_ids) {
		for (size_t i = 0; i < chan->n_decoder_types; i++)
			chan->seen_decoder_ids[i] = -1;
	} else {
		FFL(FIREFLY_ERROR_ALLOC);
	}
	chan->enc_types = types.encoder_types;
        /* /Reg */

	if (chan->auto_restrict) {
		struct firefly_channel_encoder_type *t;

		t = chan->enc_types;
		while (t) {
			t->register_func(chan->proto_encoder);
			t = t->next;
		}
	}
}

void firefly_channel_ack(struct firefly_channel *chan)
{
	struct firefly_connection *conn;
	struct firefly_channel_important_queue *tmp;

	conn = chan->conn;
	if (chan->important_id != 0 &&
			chan->conn->transport != NULL &&
			chan->conn->transport->ack != NULL)
		chan->conn->transport->ack(chan->important_id, chan->conn);
	chan->important_id = 0;
	/*
	 * If there are queued important packets and the channel is open,
	 * send the next one.
	 */
	if (chan->state == FIREFLY_CHANNEL_OPEN &&
			chan->important_queue != NULL) {
		tmp = chan->important_queue;
		chan->important_queue = tmp->next;
		conn->event_queue->offer_event_cb(conn->event_queue,
				FIREFLY_PRIORITY_HIGH,
				tmp->event, tmp->event_arg, 0, NULL);
		FIREFLY_FREE(tmp);
	}
}

bool firefly_channel_enqueue_important(struct firefly_channel *chan,
		firefly_event_execute_f event, void *event_arg)
{
	if (chan->important_id != 0) {
		struct firefly_channel_important_queue **last;

		last = &chan->important_queue;
		while (*last != NULL) {
			last = &(*last)->next;
		}
		*last = FIREFLY_MALLOC(sizeof(**last));
		(*last)->next = NULL;
		(*last)->event_arg = event_arg;
		(*last)->event = event;
		return true;
	} else {
		return false;
	}
}

void firefly_channel_raise(
		struct firefly_channel *chan, struct firefly_connection *conn,
		enum firefly_error reason, const char *msg)
{
	if (!conn && chan)
		conn = chan->conn;
	if (chan)
		chan->state = FIREFLY_CHANNEL_ERROR;
	if (conn && conn->actions && conn->actions->channel_error)
		conn->actions->channel_error(chan, reason, msg);
}

void channel_auto_restr_send_ack(struct firefly_channel *chan)
{
	firefly_protocol_channel_restrict_ack resp;

	resp.dest_chan_id   = chan->remote_id;
	resp.source_chan_id = chan->local_id;
	resp.restricted     = true;
	labcomm_encoder_ioctl(chan->conn->transport_encoder,
			      FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID,
			      &chan->important_id);
	labcomm_encode_firefly_protocol_channel_restrict_ack(
		chan->conn->transport_encoder,
		&resp);

	chan->restricted_local = true;
	channel_auto_restr_check_complete(chan);
}

void channel_auto_restr_check_complete(struct firefly_channel *chan)
{
	if (chan->restricted_local && chan->restricted_remote) {
		if (chan->conn->actions->channel_restrict_info) {
			chan->conn->actions->channel_restrict_info(
				chan, RESTRICTED);
		} else {
			/* TODO: User applicaiton is weird. */
		}
	}
}
