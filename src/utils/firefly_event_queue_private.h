/**
 * @file
 * @brief Private declarations for events.
 */

#ifndef EVENT_QUEUE_PRIVATE_H
#define EVENT_QUEUE_PRIVATE_H

#include <utils/firefly_event_queue.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief An event queue
 *
 * This is a priority queue, the events are placed in order upon insertion.
 */
struct firefly_event_queue {
	struct firefly_event *head; /**< Reference to the first event in
						the queue. */
	firefly_offer_event offer_event_cb; /**< The callback used for adding
							new events. */
	int64_t event_id; /**< Counter to keep track of used event ID's. */
	struct firefly_event **event_pool; /**< A pre-allocated pool of events. */
	size_t event_pool_size; /**< The number of events in the pool. */
	size_t event_pool_in_use; /**< The number of events currently in use in the
								event queue. */
	bool event_pool_strict_size; /**< Whether or not the pool will grow on
								   demand or keep the size constant. */
	void *context; /**< A application defined context for this queue.
							  Possibly a mutex. */
};

/**
 * @brief An event.
 *
 * @warning The largest context of an event is a single firefly_connection. A
 * larger context may (most likely) imply concurrency problems.
 */
struct firefly_event {
	unsigned char prio; /**< The priority of the event, higher value means
					higher priority. */
	int64_t id; /**< The unique identifier of this event. */
	firefly_event_execute_f execute; /**< The function to call when the
						event is executed. */
	void *context; /**< The context passed to firefly_event_execute_f() when
				the event is executed. */
	int64_t depends[FIREFLY_EVENT_QUEUE_MAX_DEPENDS]; /**< The IDs of the events
														this event depends on.
														*/
	struct firefly_event *next; /**< The next event. */
};

/**
 * @brief Takes a pre-allocated event, removes it from the pool of available
 * events and returns it.
 *
 * @param eq The queue containing the pre-allocated events
 * @return A pre-allocated event struct.
 * @retval NULL if no event was available.
 */
struct firefly_event *firefly_event_take(struct firefly_event_queue *eq);

/**
 * @brief Frees the memory used by the event
 *
 * @param ev The event to free.
 */
void firefly_event_free(struct firefly_event *ev);

/**
 * @brief Initializes an allocated event.
 *
 * @param ev The event to initialize.
 * @param id The ID to give the event.
 * @param prio The priority of the event.
 * @param execute The function called when the firefly_event is executed.
 * @param context The argument passed to the execute function when called.
 * @param nbr_depends The number of dependecies this event has.
 * @param depends The list of IDs of events this event depends on.
 */
void firefly_event_init(struct firefly_event *ev, int64_t id, unsigned char prio,
		firefly_event_execute_f execute, void *context,
		unsigned int nbr_depends, const int64_t *depends);
#endif
