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
#include <utils/firefly_errors.h>

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
 * @brief A prototype for the callback from the protocol layer called when a
 * channel request is rejected by the remote node.
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
 * @return The ID of the event freeing the channel.
 * @retval <0 if failure.
 */
int64_t firefly_channel_close(struct firefly_channel *chan);

/**
 * @brief A prototype for the callback used when a new channel is received and a
 * accept/decline decision must be done.
 *
 * @param chan The newly received channel.
 *
 * @return A boolean indicating whether the channel should be accepted
 * or not.
 * @retval true Indicates that the channel is accepted.
 * @retval false Indicates that the channal is rejected.
 */
typedef bool (* firefly_channel_accept_f)(struct firefly_channel *chan);

/**
 * @brief A prototype for the callback used to inform that a channel has been
 * opened.
 *
 * @param chan The newly opened channel.
 */
typedef void (* firefly_channel_is_open_f)(struct firefly_channel *chan);

/**
 * @brief A prototype for the callback used to inform that a channel has been
 * closed.
 *
 * @warning The channel that is passed as a parameter will be freed once
 * the callback returns. Don't keep pointers to it!
 *
 * @param chan The closed channel.
 */
typedef void (* firefly_channel_closed_f)(struct firefly_channel *chan);

/**
 * @brief A prototype of the callback used to determine how to handle request
 * for restricted mode.
 *
 * @param chan The channel to be restricted.
 */
typedef bool (* firefly_channel_restrict_f)(struct firefly_channel *chan);

/**
 * @brief A prototype of the callback used to inform about changes from/to
 * restricted mode.
 *
 * @param chan The channel that has been [un]restricted.
 */
typedef void (* firefly_channel_restrict_info_f)(struct firefly_channel *chan,
						 enum restriction_transition rinfo);

/**
 * @brief A prototype of the callback used when an error occurs on the
 * provided channel.
 *
 * @param chan The channel the error occurred on.
 * @param reason The reason an error is called.
 * @param message An optional message explaining the error.
 * @see enum firefly_error
 */
typedef void (* firefly_channel_error_f)(struct firefly_channel *chan,
		enum firefly_error reason, const char *message);

/**
 * @brief A prototype of the callback used when an error occurs on the
 * provided connection.
 *
 * @param conn The connection the error occurred on.
 * @param reason The reason an error is called.
 * @param message An optional message explaining the error.
 * @retval true If the error should propagate to the channels.
 * @retval false otherwise.
 * @see enum firefly_error
 */
typedef bool (* firefly_connection_error_f)(struct firefly_connection *conn,
		enum firefly_error reason, const char *message);

/**
 * @brief A prototype of the callback used when a new connection is
 * opened.
 *
 * @param conn The newly opened connection.
 */
typedef void (* firefly_connection_opened_f)(struct firefly_connection *conn);

/**
 * @brief Holds the callback functions that are called when there is any
 * action on channels or connections.
 *
 * Only the channel_opened callback is mandatory, the others are
 * optional to implement. It might not make sense for applications to
 * skip them but they are free to do so if they want to. If
 * channel_opened is missing, the library will segfault when it should
 * have been called.
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
 * Initializes a connection with protocol specific stuff and adds an
 * event which handles transport layer specific stuff and notifies the
 * application layer that the connection is opened.
 *
 * @param actions A struct containing the callback functions used to
 * signal different events to the application.
 * @param memory_replacements A struct containing replacements for
 * malloc/free. Ignored if set to NULL.
 * @param event_queue The event queue all events relating to this connection is
 * offered to.
 * @param tc A struct containing the transport layer specific
 * functions and data.
 *
 * @return int Indicating error if any.
 * @retval 0 if no error, negative error number otherwise.
 */
int firefly_connection_open(
		struct firefly_connection_actions *actions,
		struct firefly_memory_funcs *memory_replacements,
		struct firefly_event_queue *event_queue,
		struct firefly_transport_connection *tc);

/**
 * @brief Spawns an event that will close the connection specified by
 * the parameter.
 *
 * The event will set the state of the connection to closed and close
 * all of the channels on that connection. If there were no channels on
 * the connection, it spawns another event which will free the resources
 * for this connection. If there were channels left on the connection,
 * it will call #firefly_connection_close with the same parameter which
 * will have the effect of backing off and trying later.
 *
 * @param conn The connection to close.
 *
 * @return The ID of the first event performing the closing.
 * @retval <0 on failure.
 */
int64_t firefly_connection_close(struct firefly_connection *conn);

/**
 * @brief Get the LabComm encoder associated with the supplied channel.
 *
 * @param chan The channel to get the encoder for.
 *
 * @return The associate encoder for the channel.
 * @retval NULL Is returned if there is no encoder e.g. the channel has not been
 * accepted.
 */
struct labcomm_encoder *firefly_protocol_get_output_stream(
				struct firefly_channel *chan);

/**
 * @brief Get the LabComm decoder associated with the supplied channel.
 *
 * @param chan The channel to get the decoder for.
 *
 * @return The associate decoder for the channel.
 * @retval NULL Is returned if there is no decoder e.g. the channel has not been
 * accepted.
 */
struct labcomm_decoder *firefly_protocol_get_input_stream(
				struct firefly_channel *chan);

/**
 * @brief The number of channels in the connection.
 *
 * The complexity of this function is O(n) where n is the number of
 * channels.
 *
 * @param conn The connection to count channels on.
 * @return The number of channels open on this connection.
*/
size_t firefly_number_channels_in_connection(struct firefly_connection *conn);

/**
 * @brief Get the firefly_connection the channel is open on.
 *
 * @param chan The channel to get the connection of.
 * @return The connection the channel is open on.
 */
struct firefly_connection *firefly_channel_get_connection(
		struct firefly_channel *chan);

/**
 * @brief Gets the context of the connection.
 *
 * @param conn The connection to get the context of.
 * @return The context of the connection as a void pointer.
 */
void *firefly_connection_get_context(struct firefly_connection *conn);

/**
 * @brief Sets the context of the connection.
 *
 * @param conn The connection to set a new context on.
 * @param context The context to set on the connection.
 */
void firefly_connection_set_context(struct firefly_connection * const conn,
				    void * const context);

/**
 * @brief Gets the event queue associated with the provided connection.
 *
 * @param conn The connection to get the event queue of.
 * @return The event queue.
 */
struct firefly_event_queue *firefly_connection_get_event_queue(
	       struct firefly_connection *conn);

/**
 * @brief Request restriction of reliability and type registration on
 * encoders on channel. The agreement is not in effect until the
 * application receives the callback confirming this.
 *
 * The function spawns an event which will perform the actual sending of
 * the restriction request.
 *
 * @warning This feature if optional, it might not be implemented on all
 * hosts. Thus, it is only sensible to use this if you are in control of
 * implementing both sides of the communication!
 *
 * @param chan Channel to agree restriction upon.
 */
void firefly_channel_restrict(struct firefly_channel *chan);

/**
 * @brief Remove restriction from channel. The un-restriction is in
 * effect after confirmation is received.
 *
 * The function spawns an event which will perform the actual sending of
 * the un-restriction request.
 *
 * @warning This feature if optional, it might not be implemented on all
 * hosts. Thus, it is only sensible to use this if you are in control of
 * implementing both sides of the communication!
 *
 * @param chan Channel to agree restriction upon.
 */
void firefly_channel_unrestrict(struct firefly_channel *chan);

#endif
