#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include "utils/cppmacros.h"
#include "labcomm.h"
#include "labcomm_private.h"
#include "labcomm_ioctl.h"
#include "labcomm_static_buffer_writer.h"

#define BUFFER_SIZE (512)

static int statbuf_alloc(struct labcomm_writer *w,
		struct labcomm_writer_action_context *action_context)
{
	UNUSED_VAR(action_context);

	w->data_size = BUFFER_SIZE;
	w->count = w->data_size;
	w->data = malloc(w->data_size);
	if (w->data == NULL) {
		w->error = -ENOMEM;
	}
	w->pos = 0;

	return w->error;
}

static int statbuf_free(struct labcomm_writer *w,
		struct labcomm_writer_action_context *action_context)
{
	free(w->data);
	free(action_context);
	w->data = 0;
	w->data_size = 0;
	w->count = 0;
	w->pos = 0;
	free(w);
	return 0;
}

static int statbuf_flush(struct labcomm_writer *w,
		 struct labcomm_writer_action_context *action_context)
{
	UNUSED_VAR(action_context);

	int result = w->count - w->pos;
	return result < 0 ? -ENOMEM : result;
}

static int statbuf_start(struct labcomm_writer *w,
		 struct labcomm_writer_action_context *action_context,
		 int index,
		 const struct labcomm_signature *signature,
		 void *value)
{
	UNUSED_VAR(action_context);
	UNUSED_VAR(index);
	UNUSED_VAR(signature);
	UNUSED_VAR(value);

	w->pos = 0;
	return 0;
}

static int statbuf_end(struct labcomm_writer *w,
		struct labcomm_writer_action_context *action_context)
{
	UNUSED_VAR(w);
	UNUSED_VAR(action_context);

	return 0;
}

static int statbuf_ioctl(struct labcomm_writer *w,
		 struct labcomm_writer_action_context *action_context,
		 int index,
		 const struct labcomm_signature *signature,
		 uint32_t ioctl_action,
		 va_list arg)
{
	UNUSED_VAR(action_context);
	UNUSED_VAR(index);
	UNUSED_VAR(signature);

	int result = -ENOTSUP;
	switch (ioctl_action) {
		case LABCOMM_IOCTL_WRITER_GET_BUFFER: {
			void **buf = va_arg(arg, void**);
			size_t *size = va_arg(arg, size_t*);
			*buf = malloc(w->pos);
			*size = w->pos;
			memcpy(*buf, w->data, w->pos);
			result = 0;
			} break;
		case LABCOMM_IOCTL_WRITER_RESET_BUFFER:
			w->pos = 0;
			result = 0;
			break;
	}

	return result;
}

static const struct labcomm_writer_action action = {
	.alloc = statbuf_alloc,
	.free = statbuf_free,
	.start = statbuf_start,
	.end = statbuf_end,
	.flush = statbuf_flush,
	.ioctl = statbuf_ioctl,
};

struct labcomm_writer *labcomm_static_buffer_writer_new()
{
	struct labcomm_writer *result;
	struct labcomm_writer_action_context *ctx;

	result = malloc(sizeof(*result));
	ctx = malloc(sizeof(*ctx));
	if (result && ctx) {
		ctx->next = NULL;
		ctx->action = &action;
		ctx->context = NULL;
		result->action_context = ctx;
	} else {
		free(result);
		free(ctx);
	}
	return result;
}
