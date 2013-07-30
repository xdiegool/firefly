#ifndef FIREFLY_EVENT_QUEUE_POSIX_H
#define FIREFLY_EVENT_QUEUE_POSIX_H

#include <native/task.h>

#include <utils/firefly_event_queue.h>

/**
 * @brief Construct a new struct firefly_event_queue with with a context
 * specific for this utility.
 *
 * @param pool_size The number of preallocated events.
 * @param prio The priority of the task running the event loop.
 * @param timeout The timeout used to bound every blocking call made by any
 * thread on this event queue.
 * @return The newly contructed event queue.
 * @retval NULL on failure.
 */
struct firefly_event_queue *firefly_event_queue_xeno_new(size_t pool_size,
		int prio, RTIME timeout);

/**
 * @brief Free the specified event queue and the xenomai specific context. Stop
 * the event loop if it is running.
 *
 * @param eq The event queue to free
 */
void firefly_event_queue_xeno_free(struct firefly_event_queue **eq);

/**
 * @brief Start the event loop.
 *
 * @param eq The event queue to loop execute events from. It must have been
 * constructed with firefly_event_queue_xeno_new().
 * @return Integer indicating result.
 * @retval 0 on success.
 * @retval <0 on failure.
 */
int firefly_event_queue_xeno_run(struct firefly_event_queue *eq);

/**
 * @brief Stop the event loop. Will block untill the event loop is stopped.
 *
 * @param eq The event queue of the event loop to stop. It must have been
 * constructed with firefly_event_queue_xeno_new().
 * @return Integer indicating result.
 * @retval 0 on success.
 * @retval <0 on failure.
 */
int firefly_event_queue_xeno_stop(struct firefly_event_queue *eq);

/**
 * @brief Get the RT_TASK descriptor of the task running the event loop.
 *
 * @param eq The event queue the event loop is running on. It must have been
 * constructed with firefly_event_queue_xeno_new().
 * @return The RT_TASK of the task runnning the event loop.
 */
RT_TASK *firefly_event_queue_xeno_loop(struct firefly_event_queue *eq);
#endif
