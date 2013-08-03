#ifndef FIREFLY_TRANSPORT_RESEND_QUEUE_H
#define FIREFLY_TRANSPORT_RESEND_QUEUE_H

#include <protocol/firefly_protocol.h>

/**
 * @brief Represents a packet in the resend queue.
 */
struct resend_elem {
	unsigned char *data; /**< The data of the packet. */
	size_t size; /**< The size of the data. */
	unsigned char id; /**< The unique identifier of this packet in its queue. */
	struct timespec resend_at; /**< The absolute time when this packet must be
								 sent again. */
	long timeout; /**< The interval between resends for this packet. */
	unsigned char num_retries; /**< Number of times left this packet will be
								 sent until it is removed. */
	struct firefly_connection *conn; /**< The connection this packet comes from. */
	struct resend_elem *prev; /**< The next packet in the queue. */
};

/**
 * @brief The resend queue itself (as a single linked list).
 */
struct resend_queue {
	struct resend_elem *first; /**< The first element in the queue. */
	struct resend_elem *last; /**< The last element in the queue. */
	pthread_mutex_t lock; /**< The lock ensuring mutual exclusion when using the
							queue. */
	pthread_cond_t sig; /**< Signal used to signal when new packet is added. */
	unsigned char next_id; /**< Counter keeping track of IDs. */
};

/**
 * @brief Allocates memory and initializes the elements in the new resend queue
 * struct.
 *
 * @warning The memory allocated by this function must be freed by calling
 * #firefly_resend_queue_free, freeing in any other way is undefined behaviour
 * and should be avoided unless you know exactly what you do!
 *
 * @return A pointer to the newly allocated resend queue or NULL on failure.
 * @retval NULL on failure.
 */
struct resend_queue *firefly_resend_queue_new();

/**
 * @brief Safely frees the memory held by the provided resend queue and all of
 * its elements. This function MUST be called in order to avoid memory leaks!
 *
 * @param rq The resend queue to free.
 */
void firefly_resend_queue_free(struct resend_queue *rq);

/**
 * @brief Adds a new element to the back of the provided resend queue with the
 * specified parameters.
 *
 * @param rq      The queue to add the element to.
 * @param data    The data to resend.
 * @param size    The size of the data to resend.
 * @param timeout_ms      The time to wait before resending this packet.
 * @param retries The number of retries before giving up.
 * @param conn    The connection to resend on.
 *
 * @return The id assigned to the created resend block.
 */
unsigned char firefly_resend_add(struct resend_queue *rq,
		unsigned char *data, size_t size, long timeout_ms,
		unsigned char retries, struct firefly_connection *conn);

/**
 * @brief Removes the element from the queue with the provided ID.
 *
 * @param rq The resend queue to remove from.
 * @param id The id of the packet to remove as returned by firefly_resend_add()
 * @see firefly_resend_add()
 */
void firefly_resend_remove(struct resend_queue *rq, unsigned char id);

/**
 * @brief Free's a packet including its data.
 *
 * @param re The packet to free.
 */
void firefly_resend_elem_free(struct resend_elem *re);

/**
 * @brief Returns, but does not remove, the first element from the queue.
 *
 * @param rq The resend_queue to search through.
 *
 * @return The resend element first in the queue.
 * @retval NULL if the queue is empty.
 */
struct resend_elem *firefly_resend_top(struct resend_queue *rq);

/**
 * @brief Wait until it is time to resend the next packet and get the data
 * associated with that packet.
 *
 * @param rq The resend queue to wait on packets in.
 * @param data Result parameter, a pointer to a pointer of data. The memory will
 * be allocated and set by this function.
 * @param size Result parameter, a pointer to the variable the size of the data
 * will be saved in.
 * @param conn Result parameter. The pointed to pointer will be set to the
 * connection the returned data shall be sent on.
 * @param id Result parameter. The ID of the packet returned.
 * @param timeout Result parameter. The interval between resends in ms of the
 * packet returned.
 * @return Integer indicating result.
 * @retval 0 If a packet was returned and should be sent again.
 * @retval <0 If a packet was found but its number of retries was exceeded. In
 * this case the parameters \p data, \p size and \p id will be set
 * to \c NULL, \c 0 and \c 0 respectively and the parameter \p conn will
 * be set to the connection the packet was associated with.
 * @note The packet will remain in its place in the queue after this operation.
 * @see #firefly_resend_readd()
 */
int firefly_resend_wait(struct resend_queue *rq, unsigned char **data,
		size_t *size, struct firefly_connection **conn, unsigned char *id);

/**
 * @brief Add a timeout to a packet already in the resend queue.
 *
 * Add to queue again, add timeout to resend_at, return -1 if retries reached 0
 * @param rq The resend queue the packet is in.
 * @param id The id of the packet to add the timeout to.
 * @param timeout_ms The timeout in ms to add to the packet.
 *
 * @note This function must be called after calling #firefly_resend_wait() to
 * push the packet to the back of the queue.
 * @see #firefly_resend_wait()
 */
void firefly_resend_readd(struct resend_queue *rq, unsigned char id);

/**
 * @brief The argument to #firefly_resend_run.
 */
struct firefly_resend_loop_args {
	struct resend_queue *rq; /**< The #resend_queue. */
	void (*on_no_ack)(struct firefly_connection *conn); /**< A callback called
														  when a packet is not
														  acked in time. */
};

/**
 * @brief Run the resend loop.
 *
 * This loop will run forever and not return anything useful.
 * The loop consists of running #firefly_resend_wait(). On error call
 * #firefly_connection_error_f() in the \c struct #firefly_connection_actions of
 * the connection. For each successfully fetched packet it will be written using
 * #firefly_transport_connection_write_f() in the \c struct
 * #firefly_transport_connection of the connection and then readded with
 * #firefly_resend_readd().
 *
 * @param args The #firefly_resend_queue.
 * @return Nothing
 */
void *firefly_resend_run(void *args);
#endif
