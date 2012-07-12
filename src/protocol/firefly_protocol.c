#include "protocol/firefly_protocol_private.h"

#include <string.h>
#include <labcomm.h>
#include <errno.h>


#include <firefly_errors.h>

#define BUFFER_SIZE (128)

void labcomm_error_to_ff_error(enum labcomm_error error_id, size_t nbr_va_args,
									...)
{
	const char *err_msg = labcomm_error_get_str(error_id);
	if (err_msg == NULL) {
		err_msg = "Error with an unknown error ID occured.";
	}
	firefly_error(FIREFLY_ERROR_LABCOMM, 1, err_msg);
}

static int copy_to_writer_data(unsigned char *data,
	       	struct ff_transport_writer_data *writer_data, size_t size)
{
	// Consider alternative ways to prevent labcomm packages
	// to be fragmented.
	if (writer_data->data_size - writer_data->pos < size) {
		size_t new_size = writer_data->data_size*2;
		unsigned char *tmp = realloc(writer_data->data, new_size);
		if (tmp == NULL) {
			return -1;
		}
		writer_data->data = tmp;
		writer_data->data_size = new_size;
	}
	memcpy(data, &writer_data->data[writer_data->pos], size);
	writer_data->pos += size;
	return 0;
}

int ff_transport_writer(labcomm_writer_t *w, labcomm_writer_action_t action)
{
	struct connection  *conn = (struct connection *) w->context;
	struct ff_transport_writer_data *writer_data = conn->writer_data;
	int result = -EINVAL; // TODO Is this really right?
	switch (action) {
	case labcomm_writer_alloc: {
		w->data = malloc(BUFFER_SIZE);
		w->data_size = BUFFER_SIZE;
		w->count = BUFFER_SIZE;
		w->pos = 0;
		writer_data->data = malloc(BUFFER_SIZE);
		writer_data->data_size = BUFFER_SIZE;
		writer_data->pos = 0;
		if (writer_data->data == NULL || w->data == NULL) {
			writer_data->data_size = 0;
			w->data_size = 0;
			w->count = 0;
			w->on_error(LABCOMM_ERROR_MEMORY, 1,
					"Writer could not allocate memory.\n");
			result = -ENOMEM;
		}
   	} break;
	case labcomm_writer_free: {
		free(w->data);
		w->data = NULL;
		w->data_size = 0;
		w->count = 0;
		w->pos = 0;
		free(writer_data->data);
		writer_data->data = NULL;
		writer_data->data_size = 0;
		writer_data->pos = 0;
	} break;
	case labcomm_writer_start: {
		w->pos = 0;
		writer_data->pos = 0;
	} break;
	case labcomm_writer_continue: {
		int res = copy_to_writer_data(w->data, writer_data, w->pos);
		if (res == -1) {
			w->on_error(LABCOMM_ERROR_MEMORY, 1,
				"Writer could not save encoded data from "
								"labcomm\n");
			result = -ENOMEM;
		} else {
			w->pos = 0;
			result = 0;
		}
	} break;
	case labcomm_writer_end: {
		int res = copy_to_writer_data(w->data, writer_data, w->pos);
		if (res == -1) {
			w->on_error(LABCOMM_ERROR_MEMORY, 1,
				"Writer could not save encoded data from "
								"labcomm\n");
			result = -ENOMEM;
		} else {
			w->pos = 0;
			result = 0;
			conn->transport_write(writer_data->data, writer_data->pos, conn);
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
	/*struct connection  *conn = (struct connection *) r->context;*/
	/*struct ff_transport_writer_data *writer_data = conn->writer_data;*/
	int result = -EINVAL; // TODO Is this really right?
	switch (action) {
	case labcomm_reader_alloc: {
	} break;
	case labcomm_reader_free: {
	} break;
	case labcomm_reader_start: {
	} break;
	case labcomm_reader_continue: {
	} break;
	case labcomm_reader_end: {
	} break;
	}
	return result;
}
