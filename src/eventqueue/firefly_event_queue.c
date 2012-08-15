/**
 * @file
 * @brief Implementation of the event stuff.
 */
#include "eventqueue/firefly_event_queue_private.h"

#include <stdlib.h>

#include <firefly_errors.h>

struct firefly_event_queue *firefly_event_queue_new(
		firefly_offer_event offer_cb, void *context)
{
	struct firefly_event_queue *q = NULL;

	if ((q = malloc(sizeof(struct firefly_event_queue))) != NULL) {
		q->head = NULL;
		q->offer_event_cb = offer_cb;
		q->context = context;
	}

	return q;
}

void firefly_event_queue_free(struct firefly_event_queue **q)
{
	struct firefly_event *ev;

	while ((ev = firefly_event_pop(*q)) != NULL)
		firefly_event_free(&ev);
	free(*q);
	*q = NULL;
}

struct firefly_event *firefly_event_new(unsigned char prio,
		firefly_event_execute_f execute, void *context)
{
	struct firefly_event *ev;

	if ((ev = malloc(sizeof(struct firefly_event))) != NULL) {
		ev->prio = prio;
		ev->execute = execute;
		ev->context = context;
	}

	return ev;
}

void firefly_event_free(struct firefly_event **ev)
{
	free(*ev);
	*ev = NULL;
}

int firefly_event_add(struct firefly_event_queue *eq, struct firefly_event *ev)
{
	struct firefly_eq_node *n = eq->head;
	if (n != NULL && n->event->prio < ev->prio) {
		n = NULL;
	}

	while (n != NULL && n->next != NULL &&
			n->event->prio >= ev->prio) {
		n = n->next;
	}

	struct firefly_eq_node *node = malloc(sizeof(struct firefly_eq_node));
	if (node == NULL) {
		return -1;
	}
	node->event = ev;

	if (n != NULL) {
		node->next = n->next;
		n->next = node;
	} else {
		node->next = eq->head;
		eq->head = node;
	}

	return 0;
}

struct firefly_event *firefly_event_pop(struct firefly_event_queue *eq)
{
	// The node containing the event.
	struct firefly_eq_node *event_node;

	// The actual event
	struct firefly_event *ev;

	if ((event_node = eq->head) == NULL) {
		return NULL;
	}
	eq->head = eq->head->next;
	ev = event_node->event;
	free(event_node);

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
	firefly_event_free(&ev);
	return res;
}

size_t firefly_event_queue_length(struct firefly_event_queue *eq)
{
	struct firefly_eq_node *ev;
	size_t n = 0;

	ev = eq->head;
	while (ev) {
		ev = ev->next;
		n++;
	}

	return n;
}

void * firefly_event_queue_get_context(struct firefly_event_queue *ev)
{
	return ev->context;
}
