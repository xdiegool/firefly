#ifndef FIREFLY_EVENT_QUEUE_H
#define FIREFLY_EVENT_QUEUE_H

#include <stdlib.h>

#include "gen/firefly_protocol.h"

enum firefly_event_type {
	EVENT_CHAN_OPEN,
	EVENT_CHAN_REQ_RECV
};

/**
 * @brief A generic event.
 *
 * The largest context of an event is a single firefly_connection. A larger
 * context may imply concurrency problems.
 */
struct firefly_event_base {
	enum firefly_event_type type;
	unsigned char prio;
};

struct firefly_event {
	struct firefly_event_base base;
};

struct firefly_event_chan_open {
	struct firefly_event_base base;
	struct firefly_connection *conn;
};

struct firefly_event_chan_req_recv {
	struct firefly_event_base base;
	struct firefly_connection *conn;
	firefly_protocol_channel_request *chan_req;
};

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
