/**
 * @file
 * @brief This file contains everything related to the events of firefly.
 * 
 * Firefly is an event driven API. Most actions accesible from the public API
 * will spawn one or more events which must be run in order for the action to be
 * performed.
 * The events will be pushed to the event_queue and pop'ed from the event_queue
 * before execution.
 * All possible events are defined here.
 */

#ifndef FIREFLY_EVENT_QUEUE_H
#define FIREFLY_EVENT_QUEUE_H

#include <stdlib.h>

/* TODO: refac. */
/* TODO: comment data types and functions. */

#include "gen/firefly_protocol.h"
#include "protocol/firefly_protocol_private.h"

/**
 * @brief Each event has a type defined here.
 */
enum firefly_event_type {
	EVENT_CHAN_OPEN,
	EVENT_CHAN_CLOSED,
	EVENT_CHAN_CLOSE,
	EVENT_CHAN_REQ_RECV,
	EVENT_CHAN_RES_RECV,
	EVENT_CHAN_ACK_RECV,
	EVENT_SEND_SAMPLE,
	EVENT_RECV_SAMPLE
};

/**
 * @brief The common data structure of each event.
 */
struct firefly_event_base {
	enum firefly_event_type type; /**< The type of the event. */
	unsigned char prio; /**< The priority of the event, lower value is higher priority. */
};

/**
 * @brief A generic event.
 *
 * Warning: The largest context of an event is a single firefly_connection.
 * A larger context may imply concurrency problems.
 */
struct firefly_event {
	struct firefly_event_base base; /**< The common data structure of each
										event. */
};

/* Concrete events */

/**
 * @breif The event sending the firefly_protocol_channel_request on the
 * connection.
 */
struct firefly_event_chan_open {
	struct firefly_event_base base;
	struct firefly_connection *conn; /**< The connection the channel is opened
											on. */
	firefly_channel_rejected_f rejected_cb; /**< The callback called if the
												request was rejected by the remote
												node. */
};

/**
 * @brief The event calling the firefly_channel_closed_f callback and free'ing
 * any resources occupied by the channel.
 */
struct firefly_event_chan_closed {
	struct firefly_event_base base;
	struct firefly_channel *chan; /**< The channel to be closed. */
	struct firefly_connection *conn; /**< The connection on which the channel
										is open on. */
};

/**
 * @brief The event sending a firefly_protocol_channel_close to the remote node.
 */
struct firefly_event_chan_close {
	struct firefly_event_base base;
	struct firefly_connection *conn; /**< The connection to send the packet on. */
	firefly_protocol_channel_close *chan_close; /**< The packet to send,
													must be correctly initialized
													with id's. */
};

/**
 * @brief The event handling a received firefly_protocol_channel_request.
 */
struct firefly_event_chan_req_recv {
	struct firefly_event_base base;
	struct firefly_connection *conn; /**< The connection the request was
										received on. */
	firefly_protocol_channel_request *chan_req; /**< The received request. */
};

/**
 * @brief The event handling a received firefly_protocol_channel_response.
 */
struct firefly_event_chan_res_recv {
	struct firefly_event_base base;
	struct firefly_connection *conn; /**< The connection the request was
										received on. */
	firefly_protocol_channel_response *chan_res; /**< The received response. */
};

/**
 * @brief The event handling a received firefly_protocol_channel_ack.
 */
struct firefly_event_chan_ack_recv {
	struct firefly_event_base base;
	struct firefly_connection *conn; /**< The connection the ack was
										received on. */
	firefly_protocol_channel_ack *chan_ack; /**< The received ack. */
};

/**
 * @brief The event sending a firefly_protocol_data_sample.
 */
struct firefly_event_send_sample {
	struct firefly_event_base base;
	struct firefly_connection *conn; /**< The connection to send the sample on.
										*/
	firefly_protocol_data_sample *pkt; /**< The sample to send. */
};

/**
 * @brief The event handling a received firefly_protocol_data_sample.
 */
struct firefly_event_recv_sample {
	struct firefly_event_base base;
	struct firefly_connection *conn; /**< The connection the sample was
										received on. */
	firefly_protocol_data_sample *pkt; /**< The received sample. */
};

/* Forward declaration. */
struct firefly_event_queue;

/**
 * @brief The function implementing any logic needed in association with adding
 * an event to the event_queue.
 *
 * Should a mutex be locked or signaling be done
 * this is the funtion to implement.
 *
 * @param queue The firefly_event_queue to add the event to.
 * @param event The firefly_event to add to the firefly_event_queue.
 * @retval 0 If event was successfully added.
 * @retval != 0 if an error occured.
 */
typedef int (*firefly_offer_event)(struct firefly_event_queue *queue,
						   struct firefly_event *event);

/**
 * @brief An event queue
 */
struct firefly_event_queue {
	struct firefly_eq_node *head; /**< Reference to the first event in
									the queue. */
	struct firefly_eq_node *tail; /**< Refenrence to the last event in
									the queue. */
	firefly_offer_event offer_event_cb; /**< The callback used for adding new
					       	       	       events. */
	void *context;			/**< A application defined context for this queue.
							  Possibly a mutex.  */
};

/**
 * @brief A node in the firefly_event_queue implemented as a linked list.
 */
struct firefly_eq_node {
	struct firefly_eq_node *next; /**< The next node. */
	struct firefly_event *event; /**< The event. */
};

/**
 * @brief Initialize and alloc a new firefly_event_queue.
 *
 * @param offer_cb A function implementing firefly_offer_event.
 * @return The created firefly_event_queue.
 */
struct firefly_event_queue *firefly_event_queue_new(
		firefly_offer_event offer_cb);

/**
 * @brief Free the firefly_event_queue, will also free any events in the queue.
 *
 * If the events has any members which needs to be freed seperatly this will
 * cause memory leaks.
 * @param eq A pointer to the pionter of the firefly_event_queue to be freed.
 */
void firefly_event_queue_free(struct firefly_event_queue **eq);

/**
 * @brief Init and alloc a new event.
 *
 * @param t The type of the new event.
 * @param prio The priority of the new event.
 * @return The new event.
 * @retval NULL Upon error.
 */
struct firefly_event *firefly_event_new(enum firefly_event_type t,
					unsigned char prio);

/**
 * @brief Free an event.
 * 
 * @param ev A pointer to the pointer of the firefly_event to free.
 */
void firefly_event_free(struct firefly_event **ev);

/**
 * @brief A default implementation of adding an event to the
 * firefly_event_queue. This funciton may be set as the firefly_offer_event of
 * an event_queue if no extra functionallity is required.
 */
int firefly_event_add(struct firefly_event_queue *eq, struct firefly_event *ev);

/**
 * @brief Get the first event in the firefly_event_queue and remove it from the
 * queue.
 *
 * @param eq The queue to pop an event from.
 * @return The pop'ed event.
 */
struct firefly_event *firefly_event_pop(struct firefly_event_queue *eq);

/**
 * @brief Execute and free the event.
 *
 * The firefly_event_type of the event is checked and the parameters of the
 * event is extracted and the action is performed. All data only used by this
 * event is free'd after execution.
 *
 * @param ev The event to execute and free.
 */
int firefly_event_execute(struct firefly_event *ev);

/**
 * @brief Returns the number of events in the firefly_event_queue.
 *
 * @param eq The queue to return the length of.
 * @return The length of the queue.
 */
size_t firefly_event_queue_length(struct firefly_event_queue *eq);

#endif
