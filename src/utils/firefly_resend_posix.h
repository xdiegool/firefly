#ifndef FIREFLY_TRANSPORT_RESEND_QUEUE_H
#define FIREFLY_TRANSPORT_RESEND_QUEUE_H

#include <protocol/firefly_protocol.h>

/**
 * Represents an element in the resend queue.
 */
struct resend_elem {
	unsigned char *data;
	size_t size;
	unsigned char id;
	struct timespec resend_at;
	unsigned char num_retries;
	struct firefly_connection *conn;
	struct resend_elem *prev;
};

/**
 * The resend queue itself (as a single linked list).
 */
struct resend_queue {
	struct resend_elem *first;
	struct resend_elem *last;
	pthread_mutex_t lock;
	pthread_cond_t sig;
	unsigned char next_id;
};

/**
 * Allocates memory and initializes the elements in the new resend queue struct.
 *
 * @warning The memory allocated by this function must be freed by calling
 * #firefly_resend_queue_free, freeing in any other way is undefined behaviour
 * and should be avoided unless you know exactly what you do!
 *
 * @return A pointer to the newly allocated resend queue or NULL on failure.
 */
struct resend_queue *firefly_resend_queue_new();

/**
 * Safely frees the memory held by the provided resend queue and all of its
 * elements.  This function MUST be called in order to avoid memory leaks!
 *
 * @param rq The resend queue to free.
 */
void firefly_resend_queue_free(struct resend_queue *rq);

/**
 * Adds a new element to the back of the provided resend queue with the
 * specified parameters.
 *
 * @param rq      The queue to add the element to.
 * @param data    The data to resend.
 * @param size    The size of the data to resend.
 * @param at      The time to resend at.
 * @param retries The number of retries before giving up.
 * @param conn    The connection to resend on.
 *
 * @return The id assigned to the created resend block.
 */
unsigned char firefly_resend_add(struct resend_queue *rq,
		unsigned char *data, size_t size, long timeout_ms,
		unsigned char retries, struct firefly_connection *conn);

/**
 * Removes the element from the queue with the provided ID.
 */
void firefly_resend_remove(struct resend_queue *rq, unsigned char id);

/**
 * @brief TODO
 */
void firefly_resend_elem_free(struct resend_elem *re);

/**
 * Returns, but does not remove, the first element from the queue.
 *
 * @param rq The resend_queue to search through.
 *
 * @return The resend element first in the queue or \c NULL if the queue if
 * empty.
 */
struct resend_elem *firefly_resend_top(struct resend_queue *rq);

/**
 * @brief TODO
 */
int firefly_resend_wait(struct resend_queue *rq,
		unsigned char **data, size_t *size, struct firefly_connection **conn,
		unsigned char *id);

/**
 * @brief TODO
 *
 * Add to queue again, add timeout to resend_at, return -1 if retries reached 0
 */
void firefly_resend_readd(struct resend_queue *rq, unsigned char id,
		long timeout_ms);
#endif
