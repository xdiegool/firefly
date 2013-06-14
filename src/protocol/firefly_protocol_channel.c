#include "protocol/firefly_protocol_private.h"

#include <utils/firefly_errors.h>
#include "utils/firefly_event_queue_private.h"

struct firefly_channel *firefly_channel_new(struct firefly_connection *conn)
{
	struct firefly_channel *chan;
	struct labcomm_decoder *proto_decoder;
	struct labcomm_encoder *proto_encoder;
	struct labcomm_reader  *reader;
	struct labcomm_writer  *writer;

	chan             = FIREFLY_MALLOC(sizeof(struct firefly_channel));
	if (chan == NULL) {

		firefly_error(FIREFLY_ERROR_ALLOC, 1,
				"memory allocation failed");

		FIREFLY_FREE(chan);
		return NULL;
	}
	reader = protocol_labcomm_reader_new(conn);
	writer = protocol_labcomm_writer_new(chan);
	if (reader == NULL || writer == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
				"memory allocation failed");
		protocol_labcomm_reader_free(reader);
		protocol_labcomm_writer_free(writer);
		FIREFLY_FREE(chan);
		return NULL;
	}

	proto_decoder    = labcomm_decoder_new(reader, NULL);
	proto_encoder    = labcomm_encoder_new(writer, NULL);

	if (proto_decoder == NULL || proto_encoder == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
				"memory allocation failed");

		if (chan->proto_decoder)
			labcomm_decoder_free(chan->proto_decoder);
		if (chan->proto_encoder)
			labcomm_encoder_free(chan->proto_encoder);
		FIREFLY_FREE(chan);
		return NULL;
	}

	labcomm_register_error_handler_encoder(proto_encoder,
			labcomm_error_to_ff_error);
	labcomm_register_error_handler_decoder(proto_decoder,
			labcomm_error_to_ff_error);

	chan->local_id			= next_channel_id(conn);
	chan->remote_id			= CHANNEL_ID_NOT_SET;
	chan->important_queue	= NULL;
	chan->important_id		= 0;
	chan->current_seqno		= 0;
	chan->remote_seqno		= 0;
	chan->proto_decoder		= proto_decoder;
	chan->proto_encoder		= proto_encoder;
	chan->conn              = conn;
	chan->restricted_local		= 0;
	chan->restricted_remote		= 0;

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

int firefly_connection_enable_restricted_channels(
		struct firefly_connection *conn,
		firefly_channel_restrict_f on_channel_restrict,
		firefly_channel_restrict_info_f on_channel_restrict_info)
{
	if (!on_channel_restrict_info)
		return -1;	/* Always required. */
	conn->on_channel_restrict      = on_channel_restrict;
	conn->on_channel_restrict_info = on_channel_restrict_info;

	return 0;
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

void firefly_channel_restrict(struct firefly_channel *chan)
{
	struct firefly_connection *conn;
	int ret;

	conn = chan->conn;
	ret = conn->event_queue->offer_event_cb(conn->event_queue,
						FIREFLY_PRIORITY_MEDIUM,
						firefly_channel_restrict_event,
						chan);
	if (ret)
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "could not add event to queue");
}

int firefly_channel_restrict_event(void *earg)
{
	struct firefly_channel *chan;
	firefly_protocol_channel_restrict_request req;
	struct labcomm_encoder *tenc;

	chan = (struct firefly_channel *) earg;
	if (chan->restricted_local)
		return -1; /* Already [in the process of beeing] restricted */
	if (chan->restricted_remote)
		return 0;  /* In process of answering remote request. */
	chan->restricted_local = 1;

	req.dest_chan_id   = chan->remote_id;
	req.source_chan_id = chan->local_id;
	req.restricted     = 1;

	tenc = chan->conn->transport_encoder;
	labcomm_encoder_ioctl(tenc,
			      FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID,
			      &chan->important_id);
	labcomm_encode_firefly_protocol_channel_restrict_request(tenc, &req);

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
						chan);
	if (ret)
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "could not add event to queue");
}

int firefly_channel_unrestrict_event(void *earg)
{
	struct firefly_channel *chan;
	firefly_protocol_channel_restrict_request req;
	struct labcomm_encoder *tenc;

	chan = (struct firefly_channel *) earg;
	if (!chan->restricted_local)
		return -1; /* Tried to unrestrict unrestricted channel. */
	if (!chan->restricted_remote)
		return 0;  /* Previous request not completed.  */
	/* TODO: Handle interrupted requests better. */
	chan->restricted_local = 0;

	req.dest_chan_id   = chan->remote_id;
	req.source_chan_id = chan->local_id;
	req.restricted     = 1;

	tenc = chan->conn->transport_encoder;
	labcomm_encoder_ioctl(tenc,
			      FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID,
			      &chan->important_id);
	labcomm_encode_firefly_protocol_channel_restrict_request(tenc, &req);

	return 0;
}
