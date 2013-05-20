#define _POSIX_C_SOURCE (200112L)
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "utils/firefly_resend_posix.h"

struct resend_queue *firefly_resend_queue_new()
{
	struct resend_queue *rq = malloc(sizeof(struct resend_queue));
	if (rq == NULL) {
		// TODO error
		return NULL;
	}
	rq->next_id = 1;
	rq->first = NULL;
	rq->last = NULL;
	pthread_cond_init(&rq->sig, NULL);
	pthread_mutex_init(&rq->lock, NULL);
	return rq;
}

void firefly_resend_queue_free(struct resend_queue *rq)
{
	pthread_mutex_lock(&rq->lock);
	struct resend_elem *re = rq->first;
	struct resend_elem *tmp;

	while(re != NULL) {
		tmp = re->prev;
		free(re->data);
		free(re);
		re = tmp;
	}

	pthread_mutex_unlock(&rq->lock);
	pthread_cond_destroy(&rq->sig);
	pthread_mutex_destroy(&rq->lock);
	free(rq);
}

unsigned char firefly_resend_add(struct resend_queue *rq,
		unsigned char *data, size_t size, struct timespec at,
		unsigned char retries, struct firefly_connection *conn)
{
	struct resend_elem *re = malloc(sizeof(struct resend_elem));
	if (re == NULL) {
		return 0;
	}
	re->data = data;
	re->size = size;
	re->resend_at = at;
	re->num_retries = retries;
	re->conn = conn;
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

int firefly_resend_readd(struct resend_queue *rq, struct resend_elem *re,
		long timeout)
{
	// Decrement counter and check if it has reached 0
	if (--re->num_retries <= 0) {
		return -1;
	}

	re->resend_at.tv_nsec += timeout * 1000000;
	if (re->resend_at.tv_nsec >= 1000000000) {
		re->resend_at.tv_sec += re->resend_at.tv_nsec / 1000000000;
		re->resend_at.tv_nsec = re->resend_at.tv_nsec % 1000000000;
	}
	re->prev = NULL;
	if (rq->last == NULL) {
		rq->first = re;
	} else {
		rq->last->prev = re;
	}
	rq->last = re;
	return 0;
}

void firefly_resend_remove(struct resend_queue *rq, unsigned char id)
{
	pthread_mutex_lock(&rq->lock);
	struct resend_elem **re = &rq->first;
	while ((*re) != NULL) {
		if ((*re)->id == id) {
			struct resend_elem *tmp = *re;
			*re = (*re)->prev; 
			firefly_resend_elem_free(tmp);
		} else {
			re = &(*re)->prev;
		}
	}
	if (rq->first == NULL) {
		rq->last = NULL;
	}
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

static inline bool timespec_past(struct timespec fixed, struct timespec var)
{
	return fixed.tv_sec == var.tv_sec ?
		fixed.tv_nsec <= var.tv_nsec : fixed.tv_sec < var.tv_sec;
}

struct resend_elem *firefly_resend_wait(struct resend_queue *rq)
{
	struct resend_elem *res = NULL;
	struct timespec now;
	pthread_mutex_lock(&rq->lock);
	clock_gettime(CLOCK_REALTIME, &now);
	res = rq->first;
	while (res == NULL || !timespec_past(now, res->resend_at)) {
		pthread_cond_timedwait(&rq->sig, &rq->lock, &res->resend_at);
		clock_gettime(CLOCK_REALTIME, &now);
		res = rq->first;
	}
	rq->first = res->prev;
	pthread_mutex_unlock(&rq->lock);
	return res;
}
