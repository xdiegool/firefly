/**
 * @file
 * @brief The public API for the protocol with related functions and data
 * structures.
 */
#ifndef FIREFLY_PROTOCOL_H
#define FIREFLY_PROTOCOL_H

/**
 * @brief A structure representing a connection.
 */
struct connection {
	void *transport_conn_platspec; /**< Pointer to transport specific
						connection data. */
};

#endif
