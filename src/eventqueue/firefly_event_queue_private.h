/**
 * @file
 * @brief Private declarations for events.
 */

#ifndef EVENT_QUEUE_PRIVATE_H
#define EVENT_QUEUE_PRIVATE_H

#include <eventqueue/firefly_event_queue.h>

/**
 * @brief An event queue
 *
 * This is a priority queue, the events are placed in order upon insertion.
 */
struct firefly_event_queue {
	struct firefly_eq_node *head; /**< Reference to the first event in
						the queue. */
	firefly_offer_event offer_event_cb; /**< The callback used for adding
							new events. */
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
	firefly_event_execute_f execute; /**< The function to call when the
						event is executed. */
	void *context; /**< The context passed to firefly_event_execute_f() when
				the event is executed. */
};

/**
 * @brief A node in the prioritized firefly_event_queue implemented as a linked
 * list.
 */
struct firefly_eq_node {
	struct firefly_eq_node *next; /**< The next node. */
	struct firefly_event *event; /**< The event. */
};

#endif
