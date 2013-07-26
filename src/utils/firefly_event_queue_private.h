/**
 * @file
 * @brief Private declarations for events.
 */

#ifndef EVENT_QUEUE_PRIVATE_H
#define EVENT_QUEUE_PRIVATE_H

#include <utils/firefly_event_queue.h>

#include <stdbool.h>
#include <stdint.h>

#define FIREFLY_EVENT_QUEUE_MAX_DEPENDS (10)

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
	int64_t event_id; /**< Counter to keep track of used event ID's */
	struct firefly_event **event_pool;
	size_t event_pool_size;
	size_t event_pool_in_use;
	bool event_pool_strict_size;
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
	int64_t depends[FIREFLY_EVENT_QUEUE_MAX_DEPENDS];
	struct firefly_event *next; /**< The next event. */
};

/**
 * @brief Takes a pre-allocated event, removes it from the list of available
 * event and returns it.
 *
 * @param q The queue containing the pre-allocated events
 * @return A pre-allocated event struct.
 */
struct firefly_event *firefly_event_take(struct firefly_event_queue *q);

/**
 * @brief Frees the memory used by the event
 */
void firefly_event_free(struct firefly_event *ev);

/**
 * @brief Initializes an allocated event.
 *
 * @param ev The event to initialize.
 * @param prio The priority of the event.
 * @param execute The function called when the firefly_event is executed.
 * @param context The argument passed to the execute function when called.
 */
void firefly_event_init(struct firefly_event *ev, int64_t id, unsigned char prio,
		firefly_event_execute_f execute, void *context,
		unsigned int nbr_depends, const int64_t *depends);
#endif
