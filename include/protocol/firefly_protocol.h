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
 * @brief Opens a channel on the provided connection.
 *
 * @param conn The connection to open a channel on.
 * @return The newly created channel.
 * @retval NULL on failure.
 */
void firefly_channel_open(struct firefly_connection *conn);

/**
 * @brief Closes the channel, frees it's memory and removes it from the
 * connection's chan_list.
 *
 * @param chan The channel to close and free.
 * @param conn The connection of the channel.
 */
void firefly_channel_close(struct firefly_channel **chan,
		struct firefly_connection *conn);

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

#endif
