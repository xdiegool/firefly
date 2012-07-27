#include "eventqueue/event_queue.h"

#include <stdlib.h>

#include "protocol/firefly_protocol_private.h"
#include "firefly_errors.h"

struct firefly_event_queue *firefly_event_queue_new()
{
	struct firefly_event_queue *q = malloc(sizeof(struct firefly_event_queue));
	q->head = NULL;
	q->tail = NULL;

	return q;
}

void firefly_event_queue_free(struct firefly_event_queue **q)
{
	free(*q);
	*q = NULL;
}

struct firefly_event *firefly_event_new(enum firefly_event_type t,
										unsigned char prio)
{
	struct firefly_event *ev = malloc(sizeof(struct firefly_event));
	if (ev == NULL) {
		return NULL;
	}
	ev->base.type = t;
	ev->base.prio = prio;
	return ev;
}

void firefly_event_free(struct firefly_event **ev)
{
	free(*ev);
	*ev = NULL;
}

int firefly_event_add(struct firefly_event_queue *eq, struct firefly_event *ev)
{
	struct firefly_eq_node *node = malloc(sizeof(struct firefly_eq_node));
	if (node == NULL) {
		return -1;
	}
	node->event = ev;
	node->next = NULL;
	if (eq->tail != NULL) {
		eq->tail->next = node;
	}
	eq->tail = node;
	if (eq->head == NULL) {
		eq->head = node;
	}
	return 0;
}

struct firefly_event *firefly_event_pop(struct firefly_event_queue *eq)
{
	if(eq->head == NULL) {
		return NULL;
	}
	struct firefly_event *ev = eq->head->event;
	struct firefly_eq_node *tmp = eq->head;
	eq->head = eq->head->next;
	if (eq->tail == tmp) {
		eq->tail = NULL;
	}
	free(tmp);
	return ev;
}

int firefly_event_execute(struct firefly_event *ev)
{
	switch (ev->base.type) {
	case EVENT_CHAN_OPEN:
		firefly_channel_open_event(((struct firefly_event_chan_open *)ev)->conn);
		break;
		/* ... */
	default:
		firefly_error(FIREFLY_ERROR_ALLOC, 1, "Bad event type");
		break;
	}

	return 0;
}
