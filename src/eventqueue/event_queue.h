#ifndef FIREFLY_EVENT_QUEUE_H
#define FIREFLY_EVENT_QUEUE_H

#include <stdlib.h>

enum firefly_event_type {
	FIREFLY_EVENT_TEST
};

struct firefly_event_base {
	enum firefly_event_type type;
	unsigned char prio;
};

struct firefly_event {
	struct firefly_event_base base;
};

struct firefly_event_queue {
	struct firefly_eq_node *head;
	struct firefly_eq_node *tail;
};

struct firefly_eq_node {
	struct firefly_eq_node *next;
	struct firefly_event *event;
};

struct firefly_event_queue *firefly_event_queue_new();

void firefly_event_queue_free(struct firefly_event_queue **eq);

struct firefly_event *firefly_event_new(enum firefly_event_type t,
										unsigned char prio);

void firefly_event_free(struct firefly_event **ev);

int firefly_event_add(struct firefly_event_queue *eq, struct firefly_event *ev);

struct firefly_event *firefly_event_pop(struct firefly_event_queue *eq);

#endif
