/**
 * @file
 * @brief This file contains definitions of the event queue used by firefly.
 * 
 * Firefly is an event driven API. Most actions accesible from the public API
 * will spawn one or more events which must be run in order for the action to be
 * performed.
 * The events will be pushed to the event_queue and pop'ed from the event_queue
 * before execution.
 */

#ifndef FIREFLY_EVENT_QUEUE_H
#define FIREFLY_EVENT_QUEUE_H

#include <stdlib.h>

#include "gen/firefly_protocol.h"

/**
 * Defines different priorities.
 */
#define FIREFLY_PRIORITY_LOW (20)
#define FIREFLY_PRIORITY_MEDIUM (125)
#define FIREFLY_PRIORITY_HIGH (235)

/**
 * @brief The function called when an event is executed.
 *
 * @param event_arg The context of the event executed.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 */
typedef int (*firefly_event_execute_f)(void *event_arg);

/**
 * @brief An event.
 *
 * @warning The largest context of an event is a single firefly_connection. A
 * larger context may (most likely) imply concurrency problems.
 */
struct firefly_event {
	unsigned char prio; /**< The priority of the event, higher value means
					higher priority. */
	firefly_event_execute_f execute; /**< The function to call when the event is
									   executed. */
	void *context; /**< The context passed to firefly_event_execute_f() when
					 the event is executed. */
};

/* Forward declaration. */
struct firefly_event_queue;

/**
 * @brief The function implementing any extra logic needed to add an event to the
 * event queue.
 *
 * Should a mutex be locked or signaling be done this is the funtion to
 * implement.
 * @note This function is implemented by firefly_event_add(). But it contains no
 * logic handling mutexes nor uses the context of the firefly_event_queue.
 *
 * @param queue
 *		The firefly_event_queue to add the event to.
 * @param event
 *		The firefly_event to add to the firefly_event_queue.
 *
 * @retval 0 If event was successfully added.
 * @retval != 0 if an error occured.
 */
typedef int (*firefly_offer_event)(struct firefly_event_queue *queue,
		struct firefly_event *event);

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
 * @brief A node in the prioritized firefly_event_queue implemented as a linked
 * list.
 */
struct firefly_eq_node {
	struct firefly_eq_node *next; /**< The next node. */
	struct firefly_event *event; /**< The event. */
};

/**
 * @brief Initializes and allocates a new firefly_event_queue.
 *
 * @param offer_cb
 *		A function implementing firefly_offer_event.
 * @param context
 *		The user defined context of this firefly_event_queue.
 * @return The created firefly_event_queue.
 */
struct firefly_event_queue *firefly_event_queue_new(
		firefly_offer_event offer_cb, void *context);

/**
 * @brief Free the provided firefly_event_queue, this function will also free
 * any events left in the queue.
 *
 * @warning The context of the firefly_event_queue will not be freed.
 *
 * @warning This function calls firefly_event_free() on the remaining events in
 * the queue but that function does not free the context of specific events. If
 * the events has any context that needs to be freed separately this needs to
 * be done manually to avoid memory leaks.
 *
 * @param eq
 *		A pointer to the pionter of the firefly_event_queue to be freed.
 */
void firefly_event_queue_free(struct firefly_event_queue **eq);

/**
 * @brief Initializes and allocates a new event.
 *
 * @param prio The priority of the new event.
 * @param execute The function called when the firefly_event is executed.
 * @param context The argument passed to the execute function when called.
 * @return The new event.
 * @retval NULL upon error.
 */
struct firefly_event *firefly_event_new(unsigned char prio,
		firefly_event_execute_f execute, void *context);

/**
 * @brief Frees a generic event.
 *
 * @warning It does not frees the context of the specific event, this needs
 * to be done separately.
 *
 * @param ev
 *		A pointer to the pointer of the firefly_event to free.
 */
void firefly_event_free(struct firefly_event **ev);

/**
 * @brief A default implementation of adding an event to the
 * firefly_event_queue. The event will be sorted into the proper position.
 *
 * This function may be set as the firefly_offer_event of a firefly_event_queue
 * if no extra functionality is required.
 *
 * @warning This function is not thread safe. If thread safety is needed, this
 * function at least needs to be wrapped by a function that locks a mutex.
 *
 * @param eq The firefly_event_queue to add the firefly_event to.
 * @param ev The firefly_event to add to the firefly_event_queue.
 * @return A integer indicating the result.
 * @retval A negative integer upon error.
 */
int firefly_event_add(struct firefly_event_queue *eq, struct firefly_event *ev);

/**
 * @brief Get the first event in the firefly_event_queue and remove it from the
 * queue.
 *
 * @param eq
 *		The queue to pop an event from.
 * @return The pop'ed event.
 * @retval NULL if the firefly_event_queue is empty.
 */
struct firefly_event *firefly_event_pop(struct firefly_event_queue *eq);

/**
 * @brief Executes and frees the provided event.
 *
 * The firefly_event_type of the event is checked, the parameters of the event
 * are extracted and the action is performed. All data used by this event as
 * well as the event itself are freed after execution.
 *
 * @param ev
 *		The event to execute and free.
 * @return An integer to indicate whether the execution was successful or not.
 * @retval A negative integer upon error.
 */
int firefly_event_execute(struct firefly_event *ev);

/**
 * @brief Returns the number of events in the firefly_event_queue.
 *
 * @param eq
 *		The queue to return the length of.
 * @return The length of the queue.
 */
size_t firefly_event_queue_length(struct firefly_event_queue *eq);

#endif
