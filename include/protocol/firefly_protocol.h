/**
 * @file
 * @brief The public API for the protocol with related functions and data
 * structures.
 */
#ifndef FIREFLY_PROTOCOL_H
#define FIREFLY_PROTOCOL_H

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
 * @brief Opens a channel on the provided connection.
 *
 * @param conn The connection to open a channel on.
 * @return The newly created channel.
 * @retval NULL on failure.
 */
void firefly_channel_open(struct firefly_connection *conn);

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
