#ifndef FIREFLY_EVENT_QUEUE_H
#define FIREFLY_EVENT_QUEUE_H

#include <stdlib.h>

/* TODO: refac. */

enum firefly_event_type {
	FIREFLY_EVENT_TEST,
	EVENT_CHAN_OPEN,
	EVENT_CHAN_CLOSE
};

struct firefly_event_base {
	enum firefly_event_type type;
	unsigned char prio;
};

struct firefly_event {
	struct firefly_event_base base;
};

/* Concrete events */
struct firefly_event_chan_open {
	struct firefly_event_base base;
	struct firefly_connection *conn;
};

struct firefly_event_chan_close {
	struct firefly_event_base base;
	struct firefly_channel **chan;
	struct firefly_connection *conn; /* prev has back ref. weird though... */
};

/* Event queue */
struct firefly_event_queue;

typedef int (*firefly_offer_event)(struct firefly_event_queue *queue,
								   struct firefly_event *event);

struct firefly_event_queue {
	struct firefly_eq_node *head;
	struct firefly_eq_node *tail;
	firefly_offer_event offer_event_cb; /* The callback used for adding events. */
	void *context;						/* Possibly a mutex...  */
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

int firefly_event_execute(struct firefly_event *ev);

#endif
