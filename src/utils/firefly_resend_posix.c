#define _POSIX_C_SOURCE (200112L)
#include <pthread.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
/* #include <sys/time.h> */

#include <protocol/firefly_protocol.h>

#include "utils/firefly_resend_posix.h"
#include "protocol/firefly_protocol_private.h"

struct resend_queue *firefly_resend_queue_new()
{
	struct resend_queue *rq;

	rq = malloc(sizeof(*rq));
	if (rq) {
		rq->next_id = 1;
		rq->first = NULL;
		rq->last = NULL;
		pthread_cond_init(&rq->sig, NULL);
		pthread_mutex_init(&rq->lock, NULL);
	}

	return rq;
}

void firefly_resend_queue_free(struct resend_queue *rq)
{
	struct resend_elem *re = rq->first;
	struct resend_elem *tmp;

	while(re != NULL) {
		tmp = re->prev;
		firefly_resend_elem_free(re);
		re = tmp;
	}

	pthread_cond_destroy(&rq->sig);
	pthread_mutex_destroy(&rq->lock);
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
	struct resend_elem *re = malloc(sizeof(*re));
	if (re == NULL) {
		return 0;
	}
	re->data = data;
	re->size = size;
	clock_gettime(CLOCK_REALTIME, &re->resend_at);
	timespec_add_ms(&re->resend_at, timeout_ms);
	re->num_retries = retries;
	re->conn = conn;
	re->timeout = timeout_ms;
	re->prev = NULL;
	pthread_mutex_lock(&rq->lock);
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
	pthread_cond_signal(&rq->sig);
	pthread_mutex_unlock(&rq->lock);
	return re->id;
}

static inline struct resend_elem *firefly_resend_pop(
		struct resend_queue *rq, unsigned char id)
{
	struct resend_elem *re = rq->first;
	// If queue is empty return NULL
	if (re == NULL)
		return NULL;
	// If first elem remove it and return it
	if (re->id == id) {
		rq->first = re->prev;
		if (rq->last == re) {
			rq->last = NULL;
		}
		return re;
	}
	while (re->prev != NULL) {
		if (re->prev->id == id) {
			struct resend_elem *tmp = re->prev;
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
	pthread_mutex_lock(&rq->lock);
	struct resend_elem *re = firefly_resend_pop(rq, id);
	pthread_mutex_unlock(&rq->lock);
	if (re == NULL)
		return;

	// Decrement retries counter
	re->num_retries--;

	timespec_add_ms(&re->resend_at, re->timeout);
	re->prev = NULL;
	pthread_mutex_lock(&rq->lock);
	if (rq->last == NULL) {
		rq->first = re;
	} else {
		rq->last->prev = re;
	}
	rq->last = re;
	pthread_cond_signal(&rq->sig);
	pthread_mutex_unlock(&rq->lock);
}

void firefly_resend_remove(struct resend_queue *rq, unsigned char id)
{
	pthread_mutex_lock(&rq->lock);
	struct resend_elem *re = firefly_resend_pop(rq, id);
	if (re != NULL)
		firefly_resend_elem_free(re);
	pthread_cond_signal(&rq->sig);
	pthread_mutex_unlock(&rq->lock);
}

void firefly_resend_elem_free(struct resend_elem *re)
{
	free(re->data);
	free(re);
}

struct resend_elem *firefly_resend_top(struct resend_queue *rq)
{
	struct resend_elem *re = NULL;
	pthread_mutex_lock(&rq->lock);
	re = rq->first;
	pthread_mutex_unlock(&rq->lock);
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

	pthread_mutex_lock(&rq->lock);
	clock_gettime(CLOCK_REALTIME, &now);
	res = rq->first;
	while (res == NULL || !timespec_past(&now, &res->resend_at)) {
		if (res == NULL) {
			pthread_cond_wait(&rq->sig, &rq->lock);
		} else {
			struct timespec at = res->resend_at;
			pthread_cond_timedwait(&rq->sig, &rq->lock, &at);
		}
		clock_gettime(CLOCK_REALTIME, &now);
		res = rq->first;
	}
	*conn = res->conn;
	// Check if counter has reached 0
	if (res->num_retries <= 0) {
		firefly_resend_pop(rq, res->id);
		firefly_resend_elem_free(res);
		*data = NULL;
		*id = 0;
		*size = 0;
		result = -1;
	} else {
		*data = malloc(res->size);
		memcpy(*data, res->data, res->size);
		*size = res->size;
		*id = res->id;
		result = 0;
	}
	pthread_mutex_unlock(&rq->lock);
	return result;
}

static void firefly_resend_cleanup(void *arg)
{
	struct firefly_resend_loop_args *largs;
	struct resend_queue *rq;
	largs = arg;
	rq = largs->rq;

	pthread_mutex_unlock(&rq->lock);
	free(arg);
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
	pthread_cleanup_push(firefly_resend_cleanup, args);

	rq = largs->rq;

	while (true) {
		int prev_state;

		res = firefly_resend_wait(rq, &data, &size, &conn, &id);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &prev_state);
		if (res < 0) {
			if (largs->on_no_ack)
				largs->on_no_ack(conn);
		} else {
			conn->transport->write(data, size, conn,
					false, NULL);
			free(data);
			firefly_resend_readd(rq, id);
		}
		pthread_setcancelstate(prev_state, NULL);
	}

	pthread_cleanup_pop(1);
	return NULL;
}
