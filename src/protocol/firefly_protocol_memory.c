#include <protocol/firefly_protocol.h>
#include "protocol/firefly_protocol_private.h"

#include <labcomm.h>
#include <labcomm_private.h>

static void *ff_lc_alloc(struct labcomm_memory *m, int lifetime, 
		size_t size) 
{
	struct firefly_connection *conn;
	conn = m->context;
	if (lifetime > 0)
		return FIREFLY_RUNTIME_MALLOC(conn, size);
	else 
		return FIREFLY_MALLOC(size);
}

static void *ff_lc_realloc(struct labcomm_memory *m, int lifetime, 
		void *ptr, size_t size) 
{
	struct firefly_connection *conn;
	conn = m->context;
	if (lifetime > 0) {
		FIREFLY_RUNTIME_FREE(conn, ptr);
		return FIREFLY_RUNTIME_MALLOC(conn, size);
	} else {
		FIREFLY_FREE(ptr);
		return FIREFLY_MALLOC(size);
	}
}

static void ff_lc_free(struct labcomm_memory *m, int lifetime, 
		void *ptr)
{
	struct firefly_connection *conn;
	conn = m->context;
	if (lifetime > 0) {
		FIREFLY_RUNTIME_FREE(conn, ptr);
	} else {
		FIREFLY_FREE(ptr);
	}
}

struct labcomm_memory *firefly_labcomm_memory_new(
		struct firefly_connection *conn)
{
	struct labcomm_memory *res;
	res = FIREFLY_MALLOC(sizeof(*res));
	if (res == NULL) {
		return NULL;
	}
	res->free = ff_lc_free;
	res->alloc = ff_lc_alloc;
	res->realloc = ff_lc_realloc;
	res->context = conn;
	return res;
}

void firefly_labcomm_memory_free(struct labcomm_memory *mem)
{
	FIREFLY_FREE(mem);
}
