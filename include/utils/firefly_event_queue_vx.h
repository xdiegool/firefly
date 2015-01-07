#ifndef FIREFLY_EVENT_QUEUE_VX_H
#define FIREFLY_EVENT_QUEUE_VX_H

#include <utils/firefly_event_queue.h>

/**
 * @brief Construct a new struct firefly_event_queue with with a context
 * specific for this utility.
 *
 * @param pool_size The number of preallocated events.
 * @return The newly contructed event queue.
 */
struct firefly_event_queue *firefly_event_queue_vx_new(size_t pool_size);

/**
 * @brief Free the specified event queue and the posix specific context. Stop
 * the event loop if it is running.
 *
 * @param eq The event queue to free
 */
void firefly_event_queue_vx_free(struct firefly_event_queue **eq);

/**
 * @brief Start the event loop.
 *
 * @param eq The event queue to loop execute events from. It must have been
 * constructed with firefly_event_queue_vx_new().
 * @param attr The attributes to use when starting the event loop posix thread.
 * If NULL the default is used.
 * @return Integer indicating result.
 * @retval 0 on success.
 * @retval <0 on failure.
 */
int firefly_event_queue_vx_run(struct firefly_event_queue *eq);

/**
 * @brief Stop the event loop. Will block untill the event loop is stopped.
 *
 * @param eq The event queue of the event loop to stop. It must have been
 * constructed with firefly_event_queue_vx_new().
 * @return Integer indicating result.
 * @retval 0 on success.
 * @retval <0 on failure.
 */
int firefly_event_queue_vx_stop(struct firefly_event_queue *eq);

int64_t firefly_event_queue_vx_add(struct firefly_event_queue *eq,
		unsigned char prio, firefly_event_execute_f execute, void *context,
								   unsigned int nbr_deps, const int64_t *deps);

#endif
