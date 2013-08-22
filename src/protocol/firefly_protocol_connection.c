/**
 * @file
 * @brief Connection related functions for the protocol layer.
 */

#include <protocol/firefly_protocol.h>

#include <string.h>

#include <utils/firefly_errors.h>


#include "protocol/firefly_protocol_private.h"
#include "utils/firefly_event_queue_private.h"

static int firefly_connection_open_event(void *arg)
{
	struct firefly_connection *conn = arg;
	if (conn->transport != NULL && conn->transport->open != NULL)
		conn->transport->open(conn);
	if (conn->actions != NULL && conn->actions->connection_opened != NULL)
		conn->actions->connection_opened(conn);
	return 0;
}

struct firefly_connection *firefly_connection_new(
		struct firefly_connection_actions *actions,
		struct firefly_memory_funcs *memory_replacements,
		struct firefly_event_queue *event_queue,
		struct firefly_transport_connection *tc)
{
	struct firefly_connection *conn;
	struct labcomm_memory *lc_mem;
	struct labcomm_encoder *transport_encoder;
	struct labcomm_decoder *transport_decoder;
	struct labcomm_reader  *reader;
	struct labcomm_writer  *writer;

	conn = FIREFLY_MALLOC(sizeof(*conn));
	lc_mem = firefly_labcomm_memory_new(conn);
	if (conn == NULL || lc_mem == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
			      "memory allocation failed %s:%d",
			      __FUNCTION__, __LINE__);
		FIREFLY_FREE(conn);
		firefly_labcomm_memory_free(lc_mem);
		return NULL;
	}
	conn->actions = actions;
	reader = transport_labcomm_reader_new(conn, lc_mem);
	writer = transport_labcomm_writer_new(conn, lc_mem);
	if (reader == NULL || writer == NULL || lc_mem == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
			      "memory allocation failed %s:%d",
			      __FUNCTION__, __LINE__);
		transport_labcomm_reader_free(reader);
		transport_labcomm_writer_free(writer);
		FIREFLY_FREE(conn);
		firefly_labcomm_memory_free(lc_mem);
		return NULL;
	}
	transport_decoder = labcomm_decoder_new(reader, NULL, lc_mem, NULL);
	transport_encoder = labcomm_encoder_new(writer, NULL, lc_mem, NULL);

	if (transport_encoder == NULL || transport_decoder == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
			      "memory allocation failed %s:%d",
			      __FUNCTION__, __LINE__);
		if (transport_encoder)
			labcomm_encoder_free(transport_encoder);
		else
			transport_labcomm_writer_free(writer);
		if (transport_decoder)
			labcomm_decoder_free(transport_decoder);
		else
			transport_labcomm_reader_free(reader);
		firefly_labcomm_memory_free(lc_mem);
		FIREFLY_FREE(conn);
		return NULL;
	}
	conn->event_queue        = event_queue;
	conn->chan_list          = NULL;
	conn->channel_id_counter = 0;
	conn->transport_encoder  = transport_encoder;
	conn->transport_decoder  = transport_decoder;
	conn->lc_memory          = lc_mem;

	// TODO: Fix this once Labcomm re-gets error handling
	/* labcomm_register_error_handler_encoder(conn->transport_encoder,*/
	/*                 labcomm_error_to_ff_error);*/

	/* labcomm_register_error_handler_decoder(conn->transport_decoder,*/
	/*                 labcomm_error_to_ff_error);*/

	if (memory_replacements) {
		conn->memory_replacements.alloc_replacement =
			memory_replacements->alloc_replacement;
		conn->memory_replacements.free_replacement =
			memory_replacements->free_replacement;
	} else {
		conn->memory_replacements.alloc_replacement = NULL;
		conn->memory_replacements.free_replacement = NULL;
	}
	conn->transport = tc;
	conn->open = FIREFLY_CONNECTION_OPEN;

	reg_proto_sigs(conn->transport_encoder,
		       conn->transport_decoder,
		       conn);
	return conn;
}

int firefly_connection_open(
		struct firefly_connection_actions *actions,
		struct firefly_memory_funcs *memory_replacements,
		struct firefly_event_queue *event_queue,
		struct firefly_transport_connection *tc)
{

	struct firefly_connection *conn;
	conn = firefly_connection_new(actions, memory_replacements, event_queue, tc);
	return conn != NULL ? conn->event_queue->offer_event_cb(conn->event_queue,
			FIREFLY_PRIORITY_HIGH, firefly_connection_open_event,
			conn, 0, NULL) : -1;
}

int64_t firefly_connection_close(struct firefly_connection *conn)
{
	int ret;

	ret = conn->event_queue->offer_event_cb(conn->event_queue,
			FIREFLY_CONNECTION_CLOSE_PRIORITY, firefly_connection_close_event,
			conn, 0, NULL);
	if (ret < 0) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
			      "could not add event to queue");
	}
	return ret;
}

int firefly_connection_close_event(void *event_arg)
{
	struct firefly_connection *conn;
	struct channel_list_node *head;
	int64_t deps[FIREFLY_EVENT_QUEUE_MAX_DEPENDS] = {0};
	int count = 0;
	int64_t ret;

	conn = event_arg;
	conn->open = FIREFLY_CONNECTION_CLOSED;

	head = conn->chan_list;
	while (head != NULL && count < FIREFLY_EVENT_QUEUE_MAX_DEPENDS) {
		deps[count] = firefly_channel_close(head->chan);
		head = head->next;
		++count;
	}
	if (head == NULL) {
		ret = conn->event_queue->offer_event_cb(conn->event_queue,
				FIREFLY_CONNECTION_CLOSE_PRIORITY,
				firefly_connection_free_event, conn, count, deps);
	} else {
		ret = conn->event_queue->offer_event_cb(conn->event_queue,
				FIREFLY_CONNECTION_CLOSE_PRIORITY,
				firefly_connection_close_event,
				conn, count, deps);
	}

	if (ret < 0) {
		firefly_error(FIREFLY_ERROR_ALLOC, 1,
				  "could not add event to queue");
		return -1;
	}

	return 0;
}

int firefly_connection_free_event(void *event_arg)
{
	struct firefly_connection *conn;

	conn = event_arg;
	if (conn->transport != NULL && conn->transport->close != NULL) {
		conn->transport->close(conn);
	}
	firefly_connection_free(&conn);

	return 0;
}

void firefly_connection_free(struct firefly_connection **conn)
{
	while ((*conn)->chan_list != NULL) {
		firefly_channel_closed_event((*conn)->chan_list->chan);
	}
	FIREFLY_FREE((*conn)->chan_list);
	if ((*conn)->transport_encoder != NULL) {
		labcomm_encoder_free((*conn)->transport_encoder);
	}
	if ((*conn)->transport_decoder != NULL) {
		labcomm_decoder_free((*conn)->transport_decoder);
	}
	firefly_labcomm_memory_free((*conn)->lc_memory);
	FIREFLY_FREE(*conn);
	*conn = NULL;
}

struct firefly_channel *remove_channel_from_connection(
		struct firefly_channel *chan, struct firefly_connection *conn)
{
	struct firefly_channel *ret;
	struct channel_list_node **head;

	ret = NULL;
	head = &conn->chan_list;
	while (head != NULL && (*head) != NULL) {
		if ((*head)->chan->local_id == chan->local_id) {
			struct channel_list_node *tmp;

			tmp = (*head)->next;
			ret = chan;
			FIREFLY_FREE(*head);
			*head = tmp;
			head = NULL;
		} else {
			*head = (*head)->next;
		}
	}
	return ret;
}

int next_channel_id(struct firefly_connection *conn)
{
	return conn->channel_id_counter++;
}

void *firefly_connection_get_context(struct firefly_connection *conn)
{
	return conn->context;
}

void firefly_connection_set_context(struct firefly_connection * const conn,
				    void * const context)
{
	conn->context = context;
}

struct firefly_event_queue *firefly_connection_get_event_queue(
		struct firefly_connection *conn)
{
	return conn->event_queue;
}

struct firefly_connection_raise_arg {
	struct firefly_connection *conn;
	enum firefly_error reason;
	char msg[FF_ERRMSG_MAXLEN + 1];
};

static int firefly_connection_raise_event(void *args)
{
	struct firefly_connection_raise_arg *err_args;
	err_args = args;
	FIREFLY_CONNECTION_RAISE(err_args->conn, err_args->reason, err_args->msg);
	FIREFLY_RUNTIME_FREE(err_args->conn, args);
	return 0;
}

void firefly_connection_raise_later(struct firefly_connection *conn,
		enum firefly_error reason, const char *msg)
{
	struct firefly_connection_raise_arg *args;
	args = FIREFLY_RUNTIME_MALLOC(conn, sizeof(*args));
	if (!args) {
		FFL(FIREFLY_ERROR_ALLOC);
		return;
	}
	args->conn = conn;
	args->reason = reason;
	if (msg) {
		strncpy(args->msg, msg, FF_ERRMSG_MAXLEN);
		args->msg[FF_ERRMSG_MAXLEN] = '\0';
	} else {
		memset(args->msg, 0, FF_ERRMSG_MAXLEN + 1);
	}

	conn->event_queue->offer_event_cb(conn->event_queue,
				FIREFLY_PRIORITY_HIGH, firefly_connection_raise_event,
				args, 0, NULL);
}
