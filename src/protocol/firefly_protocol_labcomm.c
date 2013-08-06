#include <protocol/firefly_protocol.h>
#include "protocol/firefly_protocol_private.h"

#include <errno.h>
#include <stdlib.h>

#include <labcomm.h>
#include <labcomm_ioctl.h>
#include <labcomm_private.h>

#include <utils/firefly_errors.h>
#include <utils/firefly_event_queue.h>
#include <utils/cppmacros.h>

#include "utils/firefly_event_queue_private.h"

struct protocol_writer_context {
	struct firefly_channel *chan;
	bool important;
};

struct transport_writer_context {
	struct firefly_connection *conn;
	unsigned char *important_id;
};

static int proto_reader_alloc(struct labcomm_reader *r, void *context,
		    struct labcomm_decoder *decoder,
		    char *version)
{
	UNUSED_VAR(context);
	UNUSED_VAR(decoder);
	UNUSED_VAR(version);

	r->data		= NULL;
	r->data_size	= 0;
	r->count	= 0;
	r->pos		= 0;

	return 0;
}

static int proto_reader_free(struct labcomm_reader *r, void *context)
{
	UNUSED_VAR(context);

	/* TODO: Why clear members? */
	r->data		= NULL;
	r->data_size	= 0;
	r->count	= 0;
	r->pos		= 0;
	FIREFLY_FREE(r);

	return 0;
}

static int proto_reader_fill(struct labcomm_reader *r, void *context)
{
	int result;

	UNUSED_VAR(context);
	result = r->count - r->pos;

	return (result < 0 || r->data == NULL) ? -ENOMEM : result;
}

static int proto_reader_start(struct labcomm_reader *r, void *context)
{
	return proto_reader_fill(r, context);
}

static int proto_reader_end(struct labcomm_reader *r, void *context)
{
	UNUSED_VAR(r);
	UNUSED_VAR(context);

	return 0;
}

static int proto_reader_ioctl(struct labcomm_reader *r, void *context,
			      int action,
			      struct labcomm_signature *signature,
			      va_list arg)
{
	UNUSED_VAR(context);
	UNUSED_VAR(signature);

	int result;

	switch (action) {
	case FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER: {
		void *buffer;
		size_t size;

		buffer = va_arg(arg, void*);
		size = va_arg(arg, size_t);
		r->data = buffer;
		r->data_size = size;
		r->count = size;
		r->pos = 0;
		result = 0;
		} break;
	default:
		result = -ENOTSUP;
		break;
	}
	return result;
}

static void *proto_reader_mem_malloc(struct labcomm_reader *r, void *context,
		size_t size)
{
	UNUSED_VAR(r);
	return FIREFLY_RUNTIME_MALLOC((struct firefly_connection *) context, size);
}

static void proto_reader_mem_free(struct labcomm_reader *r, void *context,
		void *ptr)
{
	UNUSED_VAR(r);
	FIREFLY_RUNTIME_FREE((struct firefly_connection *) context, ptr);
}

static const struct labcomm_reader_action proto_reader_action = {
	.alloc = proto_reader_alloc,
	.free = proto_reader_free,
	.start = proto_reader_start,
	.fill = proto_reader_fill,
	.end = proto_reader_end,
	.ioctl = proto_reader_ioctl,
	.mem_malloc = proto_reader_mem_malloc,
	.mem_free = proto_reader_mem_free
};

struct labcomm_reader *protocol_labcomm_reader_new(
		struct firefly_connection *conn)
{
	struct labcomm_reader *result;

	result = FIREFLY_MALLOC(sizeof(*result));
	if (result != NULL) {
		result->context = conn;
		result->action = &proto_reader_action;
	}
	return result;
}

void protocol_labcomm_reader_free(struct labcomm_reader *r)
{
	FIREFLY_FREE(r);
}

struct labcomm_reader *transport_labcomm_reader_new(
		struct firefly_connection *conn)
{
	return protocol_labcomm_reader_new(conn);
}

void transport_labcomm_reader_free(struct labcomm_reader *r)
{
	FIREFLY_FREE(r);
}

static int comm_writer_alloc(struct labcomm_writer *w, void *context,
		struct labcomm_encoder *encoder, char *labcomm_version)
{
	UNUSED_VAR(context);
	UNUSED_VAR(encoder);
	UNUSED_VAR(labcomm_version);

	w->data_size	= BUFFER_SIZE;
	w->count	= w->data_size;
	w->data		= FIREFLY_MALLOC(w->data_size);

	if (w->data == NULL) {
		w->error = -ENOMEM;
	} else {
		memset(w->data, 0, w->data_size);
	}
	w->pos = 0;

	return w->error;
}

static int comm_writer_free(struct labcomm_writer *w, void *context)
{
	FIREFLY_FREE(w->data);
	FIREFLY_FREE(context);
	FIREFLY_FREE(w);

	return 0;
}

static int comm_writer_flush(struct labcomm_writer *w, void *context)
{
	int result;

	UNUSED_VAR(context);
	result = w->count - w->pos;

	return (result < 0) ? -ENOMEM : result;
}

static int proto_writer_start(struct labcomm_writer *w, void *context,
		struct labcomm_encoder *encoder, int index,
		struct labcomm_signature *signature, void *value)
{
	struct protocol_writer_context *ctx;

	UNUSED_VAR(encoder);
	UNUSED_VAR(index);
	UNUSED_VAR(signature);

	ctx = context;
	ctx->important = (value == NULL);
	w->pos = 0;

	if (!value && ctx->chan->restricted_local) {
		/* Until we get the updated lc, just print an error. */
		char msg[FF_ERRMSG_MAXLEN] = "Encoding signature on restricted channel: ";
		strncat(msg, signature->name, FF_ERRMSG_MAXLEN - 43);
		firefly_channel_raise(ctx->chan, NULL, FIREFLY_ERROR_PROTO_STATE, msg);

		return -EINVAL;	/* TODO: Some retval the new lc will pass. */
	}

	return 0;
}

static int proto_writer_end(struct labcomm_writer *w, void *context)
{
	struct protocol_writer_context *ctx;
	struct firefly_channel *chan;
	struct firefly_connection *conn;

	ctx  = context;
	chan = ctx->chan;
	conn = chan->conn;

	if (conn->open != FIREFLY_CONNECTION_OPEN) {
		firefly_channel_raise(ctx->chan, NULL, FIREFLY_ERROR_PROTO_STATE,
				"Cannot send data on a closed connection.");
		return -EINVAL;
	}

	// create protocol packet and encode it
	struct firefly_event_send_sample *fess =
		FIREFLY_RUNTIME_MALLOC(conn, sizeof(*fess));

	unsigned char *a = FIREFLY_RUNTIME_MALLOC(conn, w->pos);
	if (fess == NULL || a == NULL) {
		// TODO: Check if Labcomm reports error
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
				"Protocol writer could not allocate send event\n");
		FIREFLY_FREE(fess);
		FIREFLY_FREE(a);

		return -ENOMEM;
	}

	fess->chan                  = chan;
	fess->data.dest_chan_id     = chan->remote_id;
	fess->data.src_chan_id      = chan->local_id;
	fess->data.seqno            = 0;
	fess->data.important        = ctx->important;
	fess->data.app_enc_data.n_0 = w->pos;
	fess->data.app_enc_data.a   = a;
	memcpy(fess->data.app_enc_data.a, w->data, w->pos);

	conn->event_queue->offer_event_cb(conn->event_queue, FIREFLY_PRIORITY_HIGH,
			send_data_sample_event, fess, 0, NULL);
	w->pos = 0;

	return 0;
}

static int proto_writer_ioctl(struct labcomm_writer *w, void *context,
		int action, struct labcomm_signature *signature, va_list arg)
{
	UNUSED_VAR(w);
	UNUSED_VAR(context);
	UNUSED_VAR(action);
	UNUSED_VAR(signature);
	UNUSED_VAR(arg);
	int result = -ENOTSUP;
	return result;
}

static const struct labcomm_writer_action proto_writer_action = {
	.alloc = comm_writer_alloc,
	.free = comm_writer_free,
	.start = proto_writer_start,
	.end = proto_writer_end,
	.flush = comm_writer_flush,
	.ioctl = proto_writer_ioctl
};

static struct labcomm_writer *labcomm_writer_new(void *context,
		struct labcomm_writer_action const *actions)
{
	struct labcomm_writer *result;

	result = FIREFLY_MALLOC(sizeof(*result));
	if (result != NULL && context != NULL) {
		result->context = context;
		result->action = actions;
	}

	return result;
}

struct labcomm_writer *protocol_labcomm_writer_new(struct firefly_channel *chan)
{
	struct labcomm_writer *result;
	struct protocol_writer_context *context;

	context = FIREFLY_MALLOC(sizeof(*context));
	result = labcomm_writer_new(context, &proto_writer_action);
	if (context != NULL && result != NULL) {
		context->chan = chan;
	} else {
		FIREFLY_FREE(context);
		FIREFLY_FREE(result);
		result = NULL;
	}

	return result;
}

void protocol_labcomm_writer_free(struct labcomm_writer *w)
{
	if (w != NULL) {
		FIREFLY_FREE(w->context);
	}
	FIREFLY_FREE(w);
}

static int trans_writer_start(struct labcomm_writer *w, void *context,
		struct labcomm_encoder *encoder, int index,
		struct labcomm_signature *signature, void *value)
{
	UNUSED_VAR(w);
	UNUSED_VAR(context);
	UNUSED_VAR(encoder);
	UNUSED_VAR(index);
	UNUSED_VAR(signature);
	UNUSED_VAR(value);
	return 0;
}

static int trans_writer_end(struct labcomm_writer *w, void *context)
{
	struct transport_writer_context *ctx;
	ctx = (struct transport_writer_context *) context;
	ctx->conn->transport->write(w->data, w->pos, ctx->conn,
			ctx->important_id != NULL, ctx->important_id);
	ctx->important_id = NULL;
	w->pos = 0;
	return 0;
}

static int trans_writer_ioctl(struct labcomm_writer *w, void *context,
		int action, struct labcomm_signature *signature, va_list arg)
{
	struct transport_writer_context *ctx;
	int result;

	UNUSED_VAR(w);
	UNUSED_VAR(signature);
	ctx = context;

	switch (action) {
	case FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID: {
		result = 0;
		ctx->important_id = va_arg(arg, unsigned char*);
	} break;
	default:
		result = -ENOTSUP;
		break;
	}
	return result;
}

static const struct labcomm_writer_action trans_writer_action = {
	.alloc = comm_writer_alloc,
	.free = comm_writer_free,
	.start = trans_writer_start,
	.end = trans_writer_end,
	.flush = comm_writer_flush,
	.ioctl = trans_writer_ioctl
};

struct labcomm_writer *transport_labcomm_writer_new(
		struct firefly_connection *conn)
{
	struct labcomm_writer *result;
	struct transport_writer_context *context;

	context = FIREFLY_MALLOC(sizeof(*context));
	result = labcomm_writer_new(context, &trans_writer_action);
	if (result != NULL && context != NULL) {
		context->conn = conn;
		context->important_id = NULL;
	} else {
		FIREFLY_FREE(context);
		FIREFLY_FREE(result);
		result = NULL;
	}

	return result;
}

void transport_labcomm_writer_free(struct labcomm_writer *w)
{
	if (w != NULL) {
		FIREFLY_FREE(w->context);
	}
	FIREFLY_FREE(w);
}

int send_data_sample_event(void *event_arg)
{
	struct firefly_event_send_sample *fess;
	struct firefly_channel *chan;
	bool restr;

	fess = event_arg;
	chan = fess->chan;
	restr = fess->chan->restricted_local && fess->chan->restricted_remote;
	if (restr && fess->data.important) {
		firefly_channel_raise(chan, NULL, FIREFLY_ERROR_PROTO_STATE,
		       "Important sample sent on restricted channel");
	}
	if (fess->data.important && fess->chan->important_id != 0) {
		// Queue the important packet
		struct firefly_channel_important_queue **last;

		last = &fess->chan->important_queue;
		while (*last != NULL) {
			last = &(*last)->next;
		}
		*last = FIREFLY_MALLOC(sizeof(**last));
		(*last)->next = NULL;
		(*last)->fess = fess;
	} else {
		if (fess->data.important) {
			fess->data.seqno = firefly_channel_next_seqno(fess->chan);
			labcomm_encoder_ioctl(fess->chan->conn->transport_encoder,
					FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID,
					&fess->chan->important_id);
		}
		labcomm_encode_firefly_protocol_data_sample(
				fess->chan->conn->transport_encoder, &fess->data);
		FIREFLY_RUNTIME_FREE(fess->chan->conn, fess->data.app_enc_data.a);
		FIREFLY_RUNTIME_FREE(fess->chan->conn, event_arg);
	}
	return 0;
}
