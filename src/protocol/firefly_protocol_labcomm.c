#include <protocol/firefly_protocol.h>
#include "protocol/firefly_protocol_private.h"

#include <errno.h>
#include <stdlib.h>

#include <labcomm.h>
#include <labcomm_ioctl.h>
#include <labcomm_private.h>

#include <utils/firefly_errors.h>
#include <utils/firefly_event_queue.h>

#include "utils/firefly_event_queue_private.h"

struct protocol_writer_context {
	struct firefly_channel *chan;
	bool important;
};

static int proto_reader_alloc(struct labcomm_reader *r, void *context,
		    struct labcomm_decoder *decoder,
		    char *version)
{
	r->data = NULL;
	r->data_size = 0;
	r->count = 0;
	r->pos = 0;
	return 0;
}

static int proto_reader_free(struct labcomm_reader *r, void *context)
{
	r->data = NULL;
	r->data_size = 0;
	r->count = 0;
	r->pos = 0;
	return 0;
}

static int proto_reader_fill(struct labcomm_reader *r, void *context)
{
	int result = r->count - r->pos;
	return result < 0 || r->data == NULL ? -ENOMEM : result;
}

static int proto_reader_start(struct labcomm_reader *r, void *context)
{
	return proto_reader_fill(r, context);
}

static int proto_reader_end(struct labcomm_reader *r, void *context)
{
	return 0;
}

static int proto_reader_ioctl(struct labcomm_reader *r, void *context, 
								int action,
								struct labcomm_signature *signature,
								va_list arg)
{
	int result = -ENOTSUP;
	switch (action) {
	case FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER: {
		void *buffer = va_arg(arg, void*);
		int size = va_arg(arg, int);
		r->data = buffer;
		r->data_size = size;
		r->count = size;
		r->pos = 0;
		} break;
	}
	return result;
}

static const struct labcomm_reader_action proto_reader_action = {
	.alloc = proto_reader_alloc,
	.free = proto_reader_free,
	.start = proto_reader_start,
	.fill = proto_reader_fill,
	.end = proto_reader_end,
	.ioctl = proto_reader_ioctl
};

struct labcomm_reader *protocol_labcomm_reader_new()
{
	struct labcomm_reader *result;

	result = FIREFLY_MALLOC(sizeof(*result));
	if (result != NULL) {
		result->context = NULL;
		result->action = &proto_reader_action;
	}
	return result;
}

struct labcomm_reader *transport_labcomm_reader_new()
{
	return protocol_labcomm_reader_new();
}

static int proto_writer_alloc(struct labcomm_writer *w, void *context,
		struct labcomm_encoder *encoder, char *labcomm_version)
{
	w->data_size = BUFFER_SIZE;
	w->count = w->data_size;
	w->data = FIREFLY_MALLOC(w->data_size);
	if (w->data == NULL) {
		w->error = -ENOMEM;
	}
	w->pos = 0;

	return w->error;
}

static int proto_writer_free(struct labcomm_writer *w, void *context)
{
	FIREFLY_FREE(w->data);
	w->data = 0;
	w->data_size = 0;
	w->count = 0;
	w->pos = 0;

	return 0;
}

static int proto_writer_start(struct labcomm_writer *w, void *context,
		struct labcomm_encoder *encoder, int index,
		struct labcomm_signature *signature, void *value)
{
	struct protocol_writer_context *ctx;

	ctx = (struct protocol_writer_context *) context;
	ctx->important = (value == NULL);
	w->pos = 0;

	return 0;
}

static int proto_writer_end(struct labcomm_writer *w, void *context)
{
	struct protocol_writer_context *ctx;
	struct firefly_channel *chan;
	struct firefly_connection *conn;

	ctx = (struct protocol_writer_context *) context;
	chan = (struct firefly_channel *) ctx->chan;
	conn = (struct firefly_connection *) chan->conn;

	if (conn->open != FIREFLY_CONNECTION_OPEN) {
		firefly_error(FIREFLY_ERROR_PROTO_STATE, 1,
				"Cannot send data on a closed connection.\n");
		return -EINVAL;
	}

	// create protocol packet and encode it
	struct firefly_event_send_sample *fess =
		FIREFLY_RUNTIME_MALLOC(conn, sizeof(struct firefly_event_send_sample));

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
			send_data_sample_event, fess);
	w->pos = 0;

	return 0;
}

static int proto_writer_flush(struct labcomm_writer *w, void *context)
{
	int result = w->count - w->pos;
	return result < 0 ? -ENOMEM : result;
}

static int proto_writer_ioctl(struct labcomm_writer *w, void *context,
		int action, struct labcomm_signature *signature, va_list arg)
{
	int result = -ENOTSUP;
	switch (action) {
	case 0: {
		result = 0;
	} break;
	}
	return result;
}

static const struct labcomm_writer_action proto_writer_action = {
	.alloc = proto_writer_alloc,
	.free = proto_writer_free,
	.start = proto_writer_start,
	.end = proto_writer_end,
	.flush = proto_writer_flush,
	.ioctl = proto_writer_ioctl
};

struct labcomm_writer *protocol_labcomm_writer_new(struct firefly_channel *chan)
{
	struct labcomm_writer *result;
	struct protocol_writer_context *context;

	result = malloc(sizeof(*result));
	context = malloc(sizeof(*context));
	if (result != NULL && context != NULL) {
		context->chan = chan;
		result->context = context;
		result->action = &proto_writer_action;
	} else {
		FIREFLY_FREE(result);
		result = NULL;
		FIREFLY_FREE(context);
	}

	return result;
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

int ff_transport_writer(labcomm_writer_t *w, labcomm_writer_action_t action, ...)
{
	struct firefly_connection  *conn =
			(struct firefly_connection *) w->context;
	struct ff_transport_data *writer_data = conn->writer_data;
	int result = -EINVAL;
	switch (action) {
	case labcomm_writer_alloc: {
		w->data = FIREFLY_MALLOC(BUFFER_SIZE);
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
		FIREFLY_FREE(w->data);
		w->data = NULL;
		w->data_size = 0;
		w->count = 0;
		w->pos = 0;
	} break;
	case labcomm_writer_start_signature:
	case labcomm_writer_start: {
		w->pos = 0;
	} break;
	case labcomm_writer_continue_signature:
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
	case labcomm_writer_end_signature:
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
			conn->transport_write(writer_data->data, writer_data->pos, conn,
					writer_data->important_id != NULL, writer_data->important_id);
			writer_data->pos = 0;
		}
	} break;
	}
	return result;
}

int protocol_writer(labcomm_writer_t *w, labcomm_writer_action_t action, ...)
{
	struct firefly_channel *chan =
			(struct firefly_channel *) w->context;
	struct firefly_connection *conn = chan->conn;

	struct ff_transport_data *writer_data = chan->writer_data;
	int result = -EINVAL;
	bool important = false;
	switch (action) {
	case labcomm_writer_alloc: {
		w->data = FIREFLY_MALLOC(BUFFER_SIZE);
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
		FIREFLY_FREE(w->data);
		w->data = NULL;
		w->data_size = 0;
		w->count = 0;
		w->pos = 0;
	} break;
	case labcomm_writer_start_signature:
	case labcomm_writer_start: {
		w->pos = 0;
	} break;
	case labcomm_writer_continue_signature:
	case labcomm_writer_continue: {
		if (chan->conn->open != FIREFLY_CONNECTION_OPEN) {
			firefly_error(FIREFLY_ERROR_PROTO_STATE, 1,
					"Cannot send data on a closed connection.\n");
			result = -EINVAL;
		} else {
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
		}
	} break;
	case labcomm_writer_end_signature: {
		important = true;
	}
	case labcomm_writer_end: {
		if (chan->conn->open != FIREFLY_CONNECTION_OPEN) {
			firefly_error(FIREFLY_ERROR_PROTO_STATE, 1,
					"Cannot send data on a closed connection.\n");
			result = -EINVAL;
		} else {
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
				struct firefly_event_send_sample *fess =
					FIREFLY_RUNTIME_MALLOC(conn, sizeof(struct firefly_event_send_sample));
				unsigned char *a = FIREFLY_RUNTIME_MALLOC(conn, writer_data->pos);
				if (fess == NULL || a == NULL) {
					firefly_error(FIREFLY_ERROR_ALLOC, 1,
							"Protocol writer could not allocate send event\n");
				}
				fess->chan = chan;
				fess->data.dest_chan_id = chan->remote_id;
				fess->data.src_chan_id = chan->local_id;
				fess->data.seqno = 0;
				fess->data.important = important;
				fess->data.app_enc_data.n_0 = writer_data->pos;
				fess->data.app_enc_data.a = a;
				memcpy(fess->data.app_enc_data.a, writer_data->data,
						writer_data->pos);
				chan->conn->event_queue->offer_event_cb(chan->conn->event_queue,
						FIREFLY_PRIORITY_HIGH, send_data_sample_event, fess);
				writer_data->pos = 0;
			}
		}
	} break;
	}
	return result;
}

int send_data_sample_event(void *event_arg)
{
	struct firefly_event_send_sample *fess =
		(struct firefly_event_send_sample *) event_arg;

	if (fess->data.important && fess->chan->important_id != 0) {
		// Queue the important packet
		struct firefly_channel_important_queue **last =
			&fess->chan->important_queue;
		while (*last != NULL) {
			last = &(*last)->next;
		}
		*last = FIREFLY_MALLOC(sizeof(struct firefly_channel_important_queue));
		(*last)->next = NULL;
		(*last)->fess = fess;
	} else {
		if (fess->data.important) {
			fess->data.seqno = firefly_channel_next_seqno(fess->chan);
			fess->chan->conn->writer_data->important_id = &fess->chan->important_id;
		}
		labcomm_encode_firefly_protocol_data_sample(
				fess->chan->conn->transport_encoder, &fess->data);
		fess->chan->conn->writer_data->important_id = NULL;
		FIREFLY_RUNTIME_FREE(fess->chan->conn, fess->data.app_enc_data.a);
		FIREFLY_RUNTIME_FREE(fess->chan->conn, event_arg);
	}
	return 0;
}
