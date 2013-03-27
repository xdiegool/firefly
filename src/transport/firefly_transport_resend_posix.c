#include <pthread.h>
#include <stdlib.h>
#include "transport/firefly_transport_resend_posix.h"

struct resend_elem {
	unsigned char *data;
	size_t size;
	unsigned char id;
	struct timespec resend_at;
	struct resend_elem *prev;
};

struct resend_queue {
	struct resend_elem *first;
	struct resend_elem *last;
	pthread_mutex_t lock;
	pthread_cond_t sig;
	unsigned char next_id;
};

struct resend_queue *firefly_transport_resend_queue_new()
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
}

void firefly_transport_resend_queue_free(struct resend_queue *rq)
{
	pthread_cond_destroy(&rq->sig);
	pthread_mutex_destroy(&rq->lock);
	free(rq);
}

unsigned char firefly_transport_resend_add(struct resend_queue *rq,
		unsigned char *data, size_t size, struct timespec at)
{
	struct resend_elem *re = malloc(sizeof(struct resend_elem));
	if (re == NULL) {
		return 0;
	}
	re->data = data;
	re->size = size;
	re->resend_at = at;
	re->prev = NULL;
	pthread_mutex_lock(&rq->lock);
	re->id = rq->next_id++;
	if (rq->next_id == 0) {
		rq->next_id = 1;
	}
	if (rq->first == NULL) {
		rq->last = re;
	} else {
		rq->first->prev = re;
	}
	rq->first = re;
	pthread_cond_signal(&rq->sig);
	pthread_mutex_unlock(&rq->lock);
	return re->id;
}

void firefly_transport_resend_remove(struct resend_queue *rq, unsigned char id)
{
	pthread_mutex_lock(&rq->lock);
	struct resend_elem **re = &rq->last;
	while (*re != NULL) {
		if ((*re)->id == id) {
			struct resend_elem *tmp = *re;
			*re = (*re)->prev; 
			free((*re)->data);
			free((*re));
		}
		re = &(*re)->prev;
	}
	if (rq->last == NULL) {
		rq->first = NULL;
	}
	pthread_cond_signal(&rq->sig);
	pthread_mutex_unlock(&rq->lock);
}
