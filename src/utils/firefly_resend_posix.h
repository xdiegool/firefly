#ifndef FIREFLY_TRANSPORT_RESEND_QUEUE_H
#define FIREFLY_TRANSPORT_RESEND_QUEUE_H

#include <protocol/firefly_protocol.h>

struct resend_elem {
	unsigned char *data;
	size_t size;
	unsigned char id;
	struct timespec resend_at;
	unsigned char num_retries;
	struct firefly_connection *conn;
	struct resend_elem *prev;
};

struct resend_queue {
	struct resend_elem *first;
	struct resend_elem *last;
	pthread_mutex_t lock;
	pthread_cond_t sig;
	unsigned char next_id;
};

struct resend_queue *firefly_resend_queue_new();

void firefly_resend_queue_free(struct resend_queue *rq);

/**
 * @return The id assigned to the created resend block.
 */
unsigned char firefly_resend_add(struct resend_queue *rq,
		unsigned char *data, size_t size, struct timespec at,
		unsigned char retries, struct firefly_connection *conn);

void firefly_resend_remove(struct resend_queue *rq, unsigned char id);

#endif
