#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "utils/cppmacros.h"
#include "labcomm_private.h"
#include "labcomm_ioctl.h"
#include "test/labcomm_static_buffer_reader.h"

struct statbuf_context {
	void *(*m)(void *context, size_t s);
	void (*f)(void *context, void *ptr);
	void *context;
};

static int statbuf_alloc(struct labcomm_reader *r,
						 struct labcomm_reader_action_context *action_context,
						 char *labcomm_version)
{
	UNUSED_VAR(r);
	UNUSED_VAR(action_context);
	UNUSED_VAR(labcomm_version);
	return 0;
}

static int statbuf_free(struct labcomm_reader *r,
						struct labcomm_reader_action_context *action_context)
{
	r->data = NULL;
	r->data_size = 0;
	r->count = 0;
	r->pos = 0;
	free(action_context->context);
	free(action_context);
	free(r);
	return 0;
}

static int statbuf_fill(struct labcomm_reader *r,
						struct labcomm_reader_action_context *action_context)
{
	UNUSED_VAR(action_context);
	int result = r->count - r->pos;
	result = result < 0 || r->data == NULL ? -ENOMEM : result;
	return result;
}

static int statbuf_start(struct labcomm_reader *r,
						 struct labcomm_reader_action_context *action_context,
						 int local_index, int remote_index,
						 struct labcomm_signature *signature,
						 void *value)
{
	UNUSED_VAR(local_index);
	UNUSED_VAR(remote_index);
	UNUSED_VAR(signature);
	UNUSED_VAR(value);

	return statbuf_fill(r, action_context);
}

static int statbuf_end(struct labcomm_reader *r,
					   struct labcomm_reader_action_context *action_context)
{
	UNUSED_VAR(action_context);
	if (r->count <= r->pos) {
		r->data = NULL;
		r->data_size = 0;
		r->count = 0;
		r->pos = 0;
	}
	return 0;
}

static int statbuf_ioctl(struct labcomm_reader *r,
						 struct labcomm_reader_action_context *action_context,
						 int local_index,
						 int remote_index,
						 struct labcomm_signature *signature,
						 uint32_t ioctl_action,
						 va_list args)
{
	int result;

	UNUSED_VAR(action_context);
	UNUSED_VAR(local_index);
	UNUSED_VAR(remote_index);
	UNUSED_VAR(signature);

	switch (ioctl_action) {
	case LABCOMM_IOCTL_READER_SET_BUFFER: {
		void *buffer = va_arg(args, void*);
		size_t size = va_arg(args, size_t);
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

static const struct labcomm_reader_action action = {
	.alloc = statbuf_alloc,
	.free = statbuf_free,
	.start = statbuf_start,
	.fill = statbuf_fill,
	.end = statbuf_end,
	.ioctl = statbuf_ioctl,
};

struct labcomm_reader *labcomm_static_buffer_reader_new(struct labcomm_memory *m)
{
	struct labcomm_reader *result;
	struct labcomm_reader_action_context *action_context;

	result = malloc(sizeof(*result));
	action_context = malloc(sizeof(*action_context));

	if (result && action_context) {
		action_context->next = NULL;
		action_context->context = NULL;
		action_context->action = &action;
		result->action_context = action_context;
		result->memory = m;
		return result;
	} else {
		free(action_context);
		free(result);
		return NULL;
	}
}

/* struct labcomm_reader_action_context { */
/* 	struct labcomm_reader_action_context *next; */
/* 	const struct labcomm_reader_action *action; */
/* 	void *context; */
/* }; */

struct labcomm_reader *labcomm_static_buffer_reader_mem_new(void *context,
							void *(*m)(void *c, size_t s),
							void (*f)(void *c,void *p))
{
	struct labcomm_reader *result;
	struct labcomm_reader_action_context *action_context;
	struct statbuf_context *r_context;

	result = malloc(sizeof(*result));
	action_context = malloc(sizeof(*action_context));
	r_context = malloc(sizeof(*r_context));

	if (result && action_context && r_context) {
		r_context->context = context;
		r_context->m = m;
		r_context->f = f;
		action_context->next = NULL;
		action_context->action = &action;
		action_context->context = r_context;
		return result;
	} else {
		free(result);
		free (action_context);
		free(r_context);
		return NULL;
	}
}
