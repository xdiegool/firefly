#ifndef FIREFLY_TRANSPORT_RESEND_QUEUE_H
#define FIREFLY_TRANSPORT_RESEND_QUEUE_H

struct resend_queue;
struct resend_queue *firefly_transport_resend_queue_new();
void firefly_transport_resend_queue_free(struct resend_queue *rq);
unsigned char firefly_transport_resend_add(struct resend_queue *rq,
		unsigned char *data, size_t size, struct timespec at);
void firefly_transport_resend_remove(struct resend_queue *rq, unsigned char id);

#endif
