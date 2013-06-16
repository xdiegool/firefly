/**
 * @file
 * @brief The public API for the protocol with related functions and data
 * structures.
 */
#ifndef FIREFLY_PROTOCOL_H
#define FIREFLY_PROTOCOL_H

#include <labcomm.h>
#include <stdbool.h>

/**
 * @brief An opaque structure representing a connection.
 */
struct firefly_connection;

/*
 * The application must make sure the connection is no longer used after calling
 * this function. It is not thread safe to continue to use a connection after it
 * is closed. Hence the application must make use of the event queue,
 * mutexes/locks or any other feature preventing concurrency.
 */
void firefly_connection_close(struct firefly_connection *conn);

/**
 * @brief An opaque structure representing a channel.
 */
struct firefly_channel;

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
void firefly_channel_open(struct firefly_connection *conn,
		firefly_channel_rejected_f on_chan_rejected);

/**
 * @brief Offers an event to close the channel. The event will send a
 * firefly_protocol_channel_close packet, free it's memory and removes it from
 * the connection's chan_list.
 *
 * @param chan The channel to close and free.
 * @param conn The connection of the channel.
 */
void firefly_channel_close(struct firefly_channel *chan);

/**
 * @brief A prototype for a callback from the protocol layer called when a new
 * channel is received and a accept/decline descision must be done.
 *
 * @param chan The newly received channel.
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
 * @qaram chan The closed channel.
 */
typedef void (* firefly_channel_closed_f)(struct firefly_channel *chan);

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
 * @brief Type of callback used to determine how to handle request for restricted mode.
 *
 * @param chan The channel to be restricted.
 */
typedef bool (* firefly_channel_restrict_f)(struct firefly_channel *chan);

enum restriction_transition {
	UNRESTRICTED,
	RESTRICTED,
	RESTRICTION_DENIED
};

/**
 * @brief Type of callback used to inform about changes from/to restricted mode.
 *
 * @param chan The channel that has been [un]restricted.
 */
typedef void (* firefly_channel_restrict_info_f)(struct firefly_channel *chan,
						 enum restriction_transition rinfo);


/**
 * @brief Set callbacks for use when restricting channels.
 *
 * @param conn The connection to set callbacks for.
 *
 * @param on_channel_restrict_info Callback used to pass information
 *        about mode changes.
 *
 * @param on_channel_restrict Callback for incoming request.
 *        Can be null to ignore incoming requests.
 *
 * @return -1 on invalid configuration.
 */
int firefly_connection_enable_restricted_channels(
		struct firefly_connection *conn,
		firefly_channel_restrict_info_f on_channel_restrict_info,
		firefly_channel_restrict_f on_channel_restrict);

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
