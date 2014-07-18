#define _POSIX_C_SOURCE (200112L) /* TODO: Why? */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <semLib.h>
#include <taskLib.h>
#include <sysLib.h>

#include <protocol/firefly_protocol.h>
#include "utils/firefly_resend_vx.h"
#include "protocol/firefly_protocol_private.h"


struct resend_queue *firefly_resend_queue_new()
{
	struct resend_queue *rq;

	rq = malloc(sizeof(*rq));
	if (rq) {
		rq->next_id = 1;
		rq->first = NULL;
		rq->last = NULL;
		rq->lock = semMCreate(SEM_Q_PRIORITY | SEM_INVERSION_SAFE);
		rq->sig = semCCreate(0, 0);
	}

	return rq;
}

void firefly_resend_queue_free(struct resend_queue *rq)
{
	struct resend_elem *re;

	re = rq->first;
	while (re) {
		struct resend_elem *tmp;

		tmp = re->prev;
		firefly_resend_elem_free(re);
		re = tmp;
	}

	semDelete(rq->lock);
	semDelete(rq->sig);
	free(rq);
}

static inline void timespec_add_ms(struct timespec *t, long d)
{
	long long tmp;

	tmp = t->tv_nsec + d * 1000000LL;
	if (tmp >= 1000000000) {
		t->tv_sec += tmp / 1000000000;
		tmp %= 1000000000;
	}
	t->tv_nsec = tmp;
}

unsigned char firefly_resend_add(struct resend_queue *rq,
		unsigned char *data, size_t size, long timeout_ms,
		unsigned char retries, struct firefly_connection *conn)
{
	struct resend_elem *re;

	re = malloc(sizeof(*re));
	if (!re)
		return 0;

	re->data = (char *) data;
	re->size = size;
	clock_gettime(CLOCK_REALTIME, &re->resend_at);
	timespec_add_ms(&re->resend_at, timeout_ms);
	re->num_retries = retries;
	re->conn = conn;
	re->timeout = timeout_ms;
	re->prev = NULL;

	/* printf("WAITING FOR RQ LOCK\n"); */
	semTake(rq->lock, WAIT_FOREVER);
	/* printf("TOOK RQ LOCK\n"); */
	re->id = rq->next_id++;
	if (rq->next_id == 0) {
		rq->next_id = 1;
	}
	if (rq->last == NULL) {
		rq->first = re;
	} else {
		rq->last->prev = re;
	}
	rq->last = re;
	semGive(rq->lock);
	semGive(rq->sig);

	return re->id;
}

static inline struct resend_elem *firefly_resend_pop(
		struct resend_queue *rq, unsigned char id)
{
	struct resend_elem *re;

	re = rq->first;
	if (!re)
		return NULL;

	// If first elem remove it and return it
	if (re->id == id) {
		rq->first = re->prev;
		if (rq->last == re)
			rq->last = NULL;
		return re;
	}
	while (re->prev) {
		if (re->prev->id == id) {
			struct resend_elem *tmp;

			tmp = re->prev;
			re->prev = re->prev->prev;
			if (rq->last == tmp) {
				rq->last = re;
			}
			return tmp;
		} else {
			re = re->prev;
		}
	}
	return NULL;
}

void firefly_resend_readd(struct resend_queue *rq, unsigned char id)
{
	struct resend_elem *re;

	semTake(rq->lock, WAIT_FOREVER);
	re = firefly_resend_pop(rq, id);
	semGive(rq->lock);

	if (!re)
		return;

	re->num_retries--;

	timespec_add_ms(&re->resend_at, re->timeout);
	re->prev = NULL;

	semTake(rq->lock, WAIT_FOREVER);
	if (rq->last == NULL) {
		rq->first = re;
	} else {
		rq->last->prev = re;
	}
	rq->last = re;
	semGive(rq->lock);
	semGive(rq->sig);
}

void firefly_resend_remove(struct resend_queue *rq, unsigned char id)
{
	struct resend_elem *re;

	/* printf("%s\n", __func__); */
	semTake(rq->lock, WAIT_FOREVER);
	re = firefly_resend_pop(rq, id);
	if (re != NULL)
		firefly_resend_elem_free(re);
	semGive(rq->lock);
	semGive(rq->sig);
}

void firefly_resend_elem_free(struct resend_elem *re)
{
	free(re->data);
	free(re);
}

/* TODO: Looks wrong... */
struct resend_elem *firefly_resend_top(struct resend_queue *rq)
{
	struct resend_elem *re = NULL;
	
	semTake(rq->lock, WAIT_FOREVER);
	re = rq->first;
	semGive(rq->lock);

	return re;
}

static inline bool timespec_past(struct timespec *fixed, struct timespec *var)
{
	return fixed->tv_sec == var->tv_sec ?
		var->tv_nsec <= fixed->tv_nsec : var->tv_sec < fixed->tv_sec;
}

int firefly_resend_wait(struct resend_queue *rq,
		unsigned char **data, size_t *size,
		struct firefly_connection **conn,
		unsigned char *id)
{
	int result;
	struct resend_elem *res = NULL;
	struct timespec now;

	semTake(rq->lock, WAIT_FOREVER);
	clock_gettime(CLOCK_REALTIME, &now);
	res = rq->first;
	while (!res || !timespec_past(&now, &res->resend_at)) {
		/* TODO: Port of posix code. Clean it up. */
		if (!res) {
			semGive(rq->lock);
			semTake(rq->sig, WAIT_FOREVER);
			semTake(rq->lock, WAIT_FOREVER);
		} else {
			struct timespec at;
			long timeout;

			at = res->resend_at;
			timeout = ((at.tv_sec - now.tv_sec) * 1000000 +
					   (at.tv_nsec - now.tv_nsec) / 1000) / sysClkRateGet();
			semGive(rq->lock);
			semTake(rq->sig, timeout);
			semTake(rq->lock, WAIT_FOREVER);
		}
		clock_gettime(CLOCK_REALTIME, &now);
		res = rq->first;
	}
	*conn = res->conn;

	if (res->num_retries <= 0) {
		firefly_resend_pop(rq, res->id);
		firefly_resend_elem_free(res);
		*data = NULL;
		*id = 0;
		*size = 0;
		result = -1;
	} else {
		*data = malloc(res->size);
		if (!*data)
			printf("malloc failed %s:%d\n", __FILE__, __LINE__);
		memcpy(*data, res->data, res->size);
		*size = res->size;
		*id = res->id;
		result = 0;
	}
	semGive(rq->lock);

	return result;
}

void *firefly_resend_run(void *args)
{
	struct firefly_resend_loop_args *largs;
	struct resend_queue *rq;
	unsigned char *data;
	size_t size;
	struct firefly_connection *conn;
	unsigned char id;
	int res;

	largs = args;

	rq = largs->rq;

	for (;;) {
		res = firefly_resend_wait(rq, &data, &size, &conn, &id);
		taskSafe();
		if (res < 0) {
			if (largs->on_no_ack)
				largs->on_no_ack(conn);
		} else {
			conn->transport->write(data, size, conn, false, NULL);
			free(data);
			firefly_resend_readd(rq, id);
		}
		taskUnsafe();
	}

	return NULL;
}
