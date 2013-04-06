#include <protocol/firefly_protocol.h>
#include "protocol/firefly_protocol_private.h"

#include <errno.h>
#include <stdlib.h>

#include <labcomm.h>

#include <utils/firefly_errors.h>
#include <utils/firefly_event_queue.h>

#include "utils/firefly_event_queue_private.h"

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
		r->data = FIREFLY_MALLOC(BUFFER_SIZE);
		if (r->data == NULL) {
			result = -ENOMEM;
		} else {
			r->data_size = BUFFER_SIZE;
			r->pos = 0;
			r->count = 0;
		}
	} break;
	case labcomm_reader_free: {
		FIREFLY_FREE(r->data);
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
	case labcomm_reader_ioctl: {
		break;
	}
	}
	return result;
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

int ff_transport_reader(labcomm_reader_t *r, labcomm_reader_action_t action)
{
	struct firefly_connection *conn =
			(struct firefly_connection *) r->context;
	struct ff_transport_data *reader_data = conn->reader_data;
	return firefly_labcomm_reader(r, action, reader_data);
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

int protocol_reader(labcomm_reader_t *r, labcomm_reader_action_t action)
{
	struct firefly_channel *chan =
			(struct firefly_channel *) r->context;
	struct ff_transport_data *reader_data = chan->reader_data;
	return firefly_labcomm_reader(r, action, reader_data);
}
