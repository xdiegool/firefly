#ifndef FIREFLY_PROTOCOL_PRIVATE_H
#define FIREFLY_PROTOCOL_PRIVATE_H

#include <labcomm.h>

#include <protocol/firefly_protocol.h>
#include <gen/firefly_protocol.h>

/**
 * @brief Write data on the specified connection
 * This is an interface implemented by all transport layers.
 *
 * @param data The data to be written.
 * @param data_size The size of the data to be written.
 * @param conn The #firefly_connection to written the data on.
 */
typedef void (* transport_write_f)(unsigned char *data, size_t data_size,
		struct firefly_connection *conn);

/**
 * @struct ff_transport_data
 * @breif Structure describing a buffer where transport data is stored.
 */
struct ff_transport_data {
	unsigned char *data;	/**< Buffer where transport data is written. */
	size_t data_size;	/**< The size of \a data buffer. */
	size_t pos;		/**< The next position to write to. */
};

/**
 * @brief A structure representing a channel.
 */
struct firefly_channel {
	int local_chan_id; /**< The local ID used to identify this channel */
	int remote_chan_id; /**< The ID used by the remote node to identify
				this channel */
};

/**
 * @brief A structure for representing a node in a linked list of channels.
 */
struct channel_list_node {
	struct channel_list_node *next;	/**< A pointer to the next list node. */
	struct firefly_channel *chan;	/**< A pointer the the channel struct
							for this node. */
};

/**
 * @brief A structure representing a connection.
 */
struct firefly_connection {
	void *transport_conn_platspec; /**< Pointer to transport specific
						connection data. */
	struct ff_transport_data *writer_data; /**< Where the writer data is
								saved. */
	struct ff_transport_data *reader_data; /**< Where the reader data is
								saved. */
	transport_write_f transport_write; /**< Write bytes to the transport
					     layer. */
	firefly_channel_is_open_f on_channel_opened; /**< Callback, called when
							a channel has been
							opened */
	struct labcomm_encoder *transport_encoder; /**< The transport layer
							encoder for this
							connection. */
	struct labcomm_decoder *transport_decoder; /**< The transport layer
							decoder for this
							connection. */
	struct channel_list_node *chan_list; /**< The list of channels
							associated with this
							connection. */
};

/**
 * @brief The function called by the transport layer upon received data.
 * @param conn The connection the data is associated with.
 * @param data The received data.
 * @param size The size of the received data.
 */
void protocol_data_received(struct firefly_connection *conn,
	       	unsigned char *data, size_t size);

/**
 * @brief The callback registered with LabComm used to receive channel request associated packets.
 *
 * @param chan_req The decoded channel request
 * @param context The connection associated with the channel request.
 */
void handle_channel_request(firefly_protocol_channel_request *chan_req, void *context);

/**
 * @brief Send an ACK of a channel request.
 *
 * The ACK will set source_chan_id to local channel ID and dest_chan_id to
 * remote channel ID. Both must be set on the channel.
 * @param chan The channel to be set up and ACK'd.
 * @param conn The connection to send the ACK on.
 */
void protocol_channel_request_send_ack(struct firefly_channel *chan,
		struct firefly_connection *conn);

/**
 * @brief Find and return the channel associated with the given connection with the given local channel id.
 *
 * @param id The local ID of the channel.
 * @param conn The connection the channel is associated with.
 */
struct firefly_channel *find_channel_by_local_id(int id,
		struct firefly_connection *conn);

/**
 * @brief Add the channel to the connection.
 *
 * @param chan The channel to add.
 * @param conn The connection to add the channel to.
 */
void add_channel_to_connection(struct firefly_channel *chan,
		struct firefly_connection *conn);
/**
 * @brief Error callback used in LabComm that forwards the error to the \e
 * firefly_error handler that is in use.
 * @param error_id The LabComm error that occured.
 * @param nbr_va_args The number of va_args that are passed.
 * @param ... Some va_args.
 */
void labcomm_error_to_ff_error(enum labcomm_error error_id, size_t nbr_va_args,
									...);

/**
 * @brief Feeds LabComm decoder with bytes from the transport layer.
 * @param r The labcomm reader that requests data.
 * @param action What action is requested.
 * @return Value indicating how the action could be handled.
 */
int ff_transport_reader(labcomm_reader_t *r, labcomm_reader_action_t action);

/**
 * @brief Write bytes that LabComm has encoded to a memory and is then sent to
 * the transport layer.
 * @param w The labcomm writer that encodes the data.
 * @param action What action is requested.
 * @return Value indicating how the action could be handled.
 */
int ff_transport_writer(labcomm_writer_t *w, labcomm_writer_action_t action);

#endif
