/**
 * @file
 * @brief The public API for the protocol with related functions and data
 * structures.
 */
#ifndef FIREFLY_PROTOCOL_H
#define FIREFLY_PROTOCOL_H

#include <labcomm.h>
#include <stdbool.h>

#include <utils/firefly_event_queue.h>

/**
 * @brief An opaque structure representing a channel.
 */
struct firefly_channel;

/**
 * @brief An opaque structure representing a connection.
 */
struct firefly_connection;

/**
 * @brief An opaque structure that represents transport layer specific
 * connection data and that allows the protocol layer to communicate
 * with the transport layer.
 */
struct firefly_transport_connection;

/**
 * @brief Transitions between different restriction states.
 */
enum restriction_transition {
	UNRESTRICTED,
	RESTRICTED,
	RESTRICTION_DENIED
};

/**
 * @brief A function to a malloc() replacement if needed by the transport layer.
 *
 * @param conn The connection to allocate on.
 * @param size The size to allocate.
 *
 * @return A void pointer to the allocated data or NULL on failure.
 */
typedef void *(*firefly_alloc_f)(struct firefly_connection *conn, size_t size);

/**
 * @brief A function to a free() replacement if needed by the transport layer.
 *
 * @param conn The connection to free on.
 * @param p The pointer to free.
 */
typedef void (*firefly_free_f)(struct firefly_connection *conn, void *p);

/**
 * @brief Holds the memory replacement functions.
 */
struct firefly_memory_funcs {
	firefly_alloc_f alloc_replacement;
	firefly_free_f free_replacement;
};

/**
 * @brief A prototype for a callback from the protocol layer called when a
 * channel request is rejected on the remote node.
 *
 * @param conn The connection on which the channel request was rejected.
 */
typedef void (* firefly_channel_rejected_f)(struct firefly_connection *conn);

/**
 * @brief Creates and offers an event to open a channel on the provided
 * connection.
 *
 * When and if the channel is successfully opened the on_channel_opened callback
 * associated with the supplied firefly_connection is called with the opened
 * channel as argument. If the channel request was rejected by the remote node
 * the on_chan_rejected callback is called with the supplied connection as
 * argument.
 *
 * @param conn The connection to open a channel on.
 * @param on_chan_rejected A callback called if this channel request was
 * rejected by the remote node.
 */
void firefly_channel_open(struct firefly_connection *conn);

/**
 * @brief Creates and offers first a close event for the channel and
 * then a closed event for the same.
 *
 * The close event will send a firefly_protocol_channel_close packet.
 * The closed event will free the channel's memory and remove it from
 * the connection's chan_list.
 *
 * @param chan The channel to close and free.
 * @param conn The connection of the channel.
 *
 * @return TODO
 */
int64_t firefly_channel_close(struct firefly_channel *chan);

/**
 * @brief A prototype for a callback from the protocol layer called when a new
 * channel is received and a accept/decline decision must be done.
 *
 * @param chan The newly received channel.
 * @return A boolean indicating whether the channel should be accepted
 * or not.
 * @retval true Indicates that the channel is accepted.
 * @retval false Indicates that the channal is rejected.
 */
typedef bool (* firefly_channel_accept_f)(struct firefly_channel *chan);

/**
 * @brief A prototype for a callback from the protocol layer called when a
 * channel is opened.
 *
 * @param chan The newly opened channel.
 */
typedef void (* firefly_channel_is_open_f)(struct firefly_channel *chan);

/**
 * @brief A prototype for a callback from the protocol layer called when a
 * channel is closed.
 *
 * @warning The channel that is passed as a parameter will be freed once
 * the callback returns.
 *
 * @param chan The closed channel.
 */
typedef void (* firefly_channel_closed_f)(struct firefly_channel *chan);

/**
 * @brief Type of callback used to determine how to handle request for restricted mode.
 *
 * @param chan The channel to be restricted.
 */
typedef bool (* firefly_channel_restrict_f)(struct firefly_channel *chan);

/**
 * @brief Type of callback used to inform about changes from/to restricted mode.
 *
 * @param chan The channel that has been [un]restricted.
 */
typedef void (* firefly_channel_restrict_info_f)(struct firefly_channel *chan,
						 enum restriction_transition rinfo);

/**
 * @brief Called when an error occurs on the provided channel.
 *
 * Currently unused.
 *
 * TODO: Should provide some error information as well.
 */
typedef void (* firefly_channel_error_f)(struct firefly_channel *chan);

/**
 * @brief TODO
 */
typedef void (* firefly_connection_error_f)(struct firefly_connection *conn);

/**
 * @brief TODO
 */
typedef void (* firefly_connection_opened_f)(struct firefly_connection *conn);

/**
 * @brief Holds the callback functions that are called when there is any
 * action on channels or connections.
 */
struct firefly_connection_actions {
	firefly_channel_accept_f	channel_recv;		/**< Called when a new channel is received. */
	firefly_channel_is_open_f	channel_opened;		/**< Called when a channel has been opened. */
	firefly_channel_rejected_f	channel_rejected;	/**< Called if this channel could not be opened due to remote node rejected it. */
	firefly_channel_closed_f	channel_closed;		/**< Called when a channel has been closed. */
	firefly_channel_restrict_f	channel_restrict;	/**< Called on incoming restriction request. */
	firefly_channel_restrict_info_f	channel_restrict_info;	/**< Called on restriction status change. */
	firefly_channel_error_f		channel_error;		/**< Called when an error occurs on a channel. */
	firefly_connection_error_f	connection_error;	/**< Called when an error occurs on a connection. */
	firefly_connection_opened_f connection_opened;	/**< Called when a connection is opened.  */
};


/**
 * Initializes a connection with protocol specific stuff.
 *
 * @param on_channel_opened Callback for when a channel has been opened.
 * @param on_channel_closed Callback for when a channel has been closed.
 * @param on_channel_recv Callback for when a channel has been recveived.
 * @param transport_write The interface between transport and protocol layer for
 * writing data.
 * @param transport_ack The interface between transport and protocol layer for
 * removing elements in the resend queue.
 * @param event_queue The event queue all events relating to this connection is
 * offered to.
 * @param plat_spec Transport layer specifik data.
 * @param plat_spec_free The funtion responsible for freeing the transport
 * specifik data.
 *
 * @return int Indicating error if any.
 * @retval 0 if no error, negative error number otherwise.
 */
int firefly_connection_open(
		struct firefly_connection_actions *actions,
		struct firefly_memory_funcs *memory_replacements,
		struct firefly_event_queue *event_queue,
		struct firefly_transport_connection *tc);

/*
 * The application must make sure the connection is no longer used after calling
 * this function. It is not thread safe to continue to use a connection after it
 * is closed. Hence the application must make use of the event queue,
 * mutexes/locks or any other feature preventing concurrency.
 *
 * @return TODO
 */
int64_t firefly_connection_close(struct firefly_connection *conn);

/**
 * @brief Get the LabComm encoder associated with the supplied channel.
 * @param chan The channel to get the encoder for.
 * @return The associate encoder for the channel.
 * @retval NULL Is returned if there is no encoder e.g. the channel has not been
 * accepted.
 */
struct labcomm_encoder *firefly_protocol_get_output_stream(
				struct firefly_channel *chan);

/**
 * @brief Get the LabComm decoder associated with the supplied channel.
 * @param chan The channel to get the decoder for.
 * @return The associate decoder for the channel.
 * @retval NULL Is returned if there is no decoder e.g. the channel has not been
 * accepted.
 */
struct labcomm_decoder *firefly_protocol_get_input_stream(
				struct firefly_channel *chan);

/**
 * @brief The number of channels in the connection.
 *
 * @param The connection
 * @return The number of channels open on this connection.
*/
size_t firefly_number_channels_in_connection(struct firefly_connection *conn);

/**
 * @brief Get the firefly_connection the channel is open on.
 *
 * @param The channel.
 * @return The connection the channel is open on.
 */
struct firefly_connection *firefly_channel_get_connection(
		struct firefly_channel *chan);

/**
 * @brief Gets the context of the connection.
 * @param The connection.
 */
void *firefly_connection_get_context(struct firefly_connection *conn);

/**
 * @brief Sets the context of the connection.
 * @param The connection.
 * @param The context.
 */
void firefly_connection_set_context(struct firefly_connection * const conn,
				    void * const context);

/**
 * @brief Gets the event queue associated with the connection.
 * @param The connection.
 * @return The event queue.
 */
struct firefly_event_queue *firefly_connection_get_event_queue(
	       struct firefly_connection *conn);

/**
 * @brief Request restriction of reliability and type registration
 *        on encoders on channel. The agreement is not in effect until
 *        the application receives the callback confirming this.
 * @param chan Channel to agree restriction upon.
 */
void firefly_channel_restrict(struct firefly_channel *chan);

/**
 * @brief Rise channel from restricted mode. In effect after confirmation.
 * @param chan Channel to agree restriction upon.
 */
void firefly_channel_unrestrict(struct firefly_channel *chan);

#endif
