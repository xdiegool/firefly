/**
 * @file
 * @brief The public API for the protocol with related functions and data
 * structures.
 */
#ifndef FIREFLY_PROTOCOL_H
#define FIREFLY_PROTOCOL_H

/**
 * @breif Structure describing a buffer where transport data is stored.
 */
struct ff_transport_data {
	unsigned char *data;	/**< Buffer where transport data is written. */
	size_t data_size;	/**< The size of \a data buffer. */
	size_t pos;		/**< The next position to write to. */
};

struct connection; // Forward declaration.

/**
 * @brief Write data on the specified connection
 * This is an interface implemented by all transport layers.
 *
 * @param data The data to be written.
 * @param data_size The size of the data to be written.
 * @param conn The connection to written the data on.
 */
typedef void (* transport_write_f)(unsigned char *data, size_t data_size,
		struct connection *conn);

/**
 * @brief A structure representing a connection.
 */
struct connection {
	void *transport_conn_platspec; /**< Pointer to transport specific
						connection data. */
	struct ff_transport_data *writer_data; /**< Where the writer data is
								saved. */
	struct ff_transport_data *reader_data; /**< Where the reader data is
								saved. */
	transport_write_f transport_write; /**< Write bytes to the transport
					     layer. */
};

#endif
