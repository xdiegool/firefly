/**
 * @file
 * @brief The public API for the protocol with related functions and data
 * structures.
 */
#ifndef FIREFLY_PROTOCOL_H
#define FIREFLY_PROTOCOL_H

#include <labcomm.h>

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

#endif
