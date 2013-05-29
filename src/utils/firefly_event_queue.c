/**
 * @file
 * @brief Implementation of the event stuff.
 */
#include "utils/firefly_event_queue_private.h"

#include <stdlib.h>
#include <stdbool.h>

#include <utils/firefly_errors.h>
#include "protocol/firefly_protocol_private.h"

struct firefly_event_queue *firefly_event_queue_new(
		firefly_offer_event offer_cb, size_t pool_size, void *context)
{
	struct firefly_event_queue *q = NULL;

	if ((q = FIREFLY_MALLOC(sizeof(struct firefly_event_queue))) != NULL) {
		q->head = NULL;
		q->offer_event_cb = offer_cb;
		q->context = context;
		q->event_pool = FIREFLY_MALLOC(sizeof(struct firefly_event *)*pool_size);
		for (size_t i = 0; i < pool_size; i++) {
			q->event_pool[i] = FIREFLY_MALLOC(sizeof(struct firefly_event));
		}
		q->event_pool_size = pool_size;
		q->event_pool_in_use = 0;
		q->event_pool_strict_size = false;
	}

	return q;
}

void firefly_event_queue_free(struct firefly_event_queue **q)
{
	struct firefly_event *ev;

	while ((ev = firefly_event_pop(*q)) != NULL)
		firefly_event_return(*q, &ev);
	for (size_t i = 0; i < (*q)->event_pool_size; i++) {
		firefly_event_free((*q)->event_pool[i]);
	}
	FIREFLY_FREE((*q)->event_pool);
	FIREFLY_FREE(*q);
	*q = NULL;
}

void firefly_event_queue_set_strict_pool_size(struct firefly_event_queue *eq,
		bool strict_size)
{
	eq->event_pool_strict_size = strict_size;
}

struct firefly_event *firefly_event_new(unsigned char prio,
		firefly_event_execute_f execute, void *context)
{
	struct firefly_event *ev;

	if ((ev = FIREFLY_MALLOC(sizeof(struct firefly_event))) != NULL) {
		ev->prio = prio;
		ev->execute = execute;
		ev->context = context;
	}

	return ev;
}

void firefly_event_free(struct firefly_event *ev)
{
	FIREFLY_FREE(ev);
}

void firefly_event_init(struct firefly_event *ev, unsigned char prio,
		firefly_event_execute_f execute, void *context)
{
		ev->prio = prio;
		ev->execute = execute;
		ev->context = context;
}

struct firefly_event *firefly_event_take(struct firefly_event_queue *q)
{
	if (q->event_pool_in_use > q->event_pool_size) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
				"EVENT: Inconsistent state, more events in use than possible.");
		return NULL;
	} else if (q->event_pool_in_use == q->event_pool_size) {
		if (q->event_pool_strict_size) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
				"No available events in the pool.");
			return NULL;
		} else {
			// Double the size of the new pool
			size_t new_size = q->event_pool_size*2;
			struct firefly_event **new_pool =
				FIREFLY_MALLOC(sizeof(struct firefly_event*)*new_size);
			memcpy(new_pool, q->event_pool, q->event_pool_size);
			for (size_t i = q->event_pool_in_use; i < new_size; i++) {
				new_pool[i] = FIREFLY_MALLOC(sizeof(struct firefly_event));
			}
			FIREFLY_FREE(q->event_pool);
			q->event_pool = new_pool;
			q->event_pool_size = new_size;
		}
	}
	struct firefly_event *ev = q->event_pool[q->event_pool_in_use];
	q->event_pool[q->event_pool_in_use] = NULL;
	q->event_pool_in_use++;

	return ev;
}

void firefly_event_return(struct firefly_event_queue *q,
		struct firefly_event **ev)
{
	if (q->event_pool_in_use > q->event_pool_size) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
				"EVENT: Inconsistent state, more events in use than possible.");
	} else {
		q->event_pool_in_use--;
		q->event_pool[q->event_pool_in_use] = *ev;
		*ev = NULL;
	}
}

int firefly_event_add(struct firefly_event_queue *eq, unsigned char prio,
		firefly_event_execute_f execute, void *context)
{
	struct firefly_event **n = &eq->head;

	// Find the node to insert the event before, it may be NULL and eq->head
	while (*n != NULL && (*n)->prio >= prio) {
		n = &(*n)->next;
	}

	struct firefly_event *ev = firefly_event_take(eq);
	if (ev == NULL) {
		return -1;
	}
	firefly_event_init(ev, prio, execute, context);
	ev->next = (*n);
	*n = ev;

	return 0;
}

struct firefly_event *firefly_event_pop(struct firefly_event_queue *eq)
{
	// The actual event
	struct firefly_event *ev;

	if ((ev = eq->head) == NULL) {
		return NULL;
	}
	eq->head = eq->head->next;

	return ev;
}

int firefly_event_execute(struct firefly_event *ev)
{
	int res;
	if (ev->execute == NULL) {
		firefly_error(FIREFLY_ERROR_EVENT, 1, "Bad event type");
		res = -1;
	} else {
		res = ev->execute(ev->context);
	}
	return res;
}

size_t firefly_event_queue_length(struct firefly_event_queue *eq)
{
	struct firefly_event *ev;
	size_t n = 0;

	ev = eq->head;
	while (ev) {
		ev = ev->next;
		n++;
	}

	return n;
}

void *firefly_event_queue_get_context(struct firefly_event_queue *eq)
{
	return eq->context;
}
