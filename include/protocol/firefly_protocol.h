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
 *brief The number of channels in the connection.
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

#endif
