#ifndef FIREFLY_PROTOCOL_PRIVATE_H
#define FIREFLY_PROTOCOL_PRIVATE_H

#include <labcomm.h>
#include <signal.h>

#include <protocol/firefly_protocol.h>
#include <gen/firefly_protocol.h>

#include "utils/firefly_event_queue.h"

#define CHANNEL_ID_NOT_SET (-1)
#define BUFFER_SIZE 128
#define FIREFLY_CONNECTION_CLOSED	(0)
#define FIREFLY_CONNECTION_OPEN	(1)

void reg_proto_sigs(struct labcomm_encoder *enc,
					struct labcomm_decoder *dec,
					struct firefly_connection *conn);

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

typedef void (*transport_connection_free)(struct firefly_connection *conn);

typedef void (* protocol_data_received_f)(struct firefly_connection *conn,
					  unsigned char *data,
					  size_t size);

/**
 * @brief Structure describing a buffer where transport data is stored.
 */
struct ff_transport_data {
	unsigned char *data;	/**< Buffer where transport data is written. */
	size_t data_size;	/**< The size of \a data buffer. */
	size_t pos;		/**< The next position to write to. */
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
	transport_connection_free transport_conn_platspec_free;
	sig_atomic_t open; /**< The flag indicating the opened state of a
						 connection. */
	struct ff_transport_data *writer_data; /**< Where the writer data is
								saved. */
	struct ff_transport_data *reader_data; /**< Where the reader data is
								saved. */
	transport_write_f transport_write; /**< Write bytes to the transport
							layer. */
	firefly_channel_is_open_f on_channel_opened; /**< Callback, called when
							a channel has been
							opened */
	firefly_channel_closed_f on_channel_closed; /**< Callback, called when
							a channel has been
							closed */
	firefly_channel_accept_f on_channel_recv; /**< Callback, called when a
							channel is received to
							decide if it is accepted or
							rejected. */
	struct labcomm_encoder *transport_encoder; /**< The transport layer
							encoder for this
							connection. */
	struct labcomm_decoder *transport_decoder; /**< The transport layer
							decoder for this
							connection. */
	struct channel_list_node *chan_list; /**< The list of channels
							associated with this
							connection. */
	struct firefly_event_queue *event_queue; /**< The queue to which spawned events are added. */
	int channel_id_counter; /**< The unique id reserved to the next opened
								channel on the connection. */
	void *context; /**< A reference to an optional, user defined context.  */
};

/**
 * @brief A structure representing a channel.
 */
struct firefly_channel {
	struct firefly_connection *conn; /**< The connection the channel exists
									on. */
	int local_id; /**< The local ID used to identify this channel */
	int remote_id; /**< The ID used by the remote node to identify
				this channel */
	struct labcomm_encoder *proto_encoder; /**< LabComm encoder for this
					   			channel.*/
	struct labcomm_decoder *proto_decoder; /**< LabComm decoder for this
					   			channel. */
	struct ff_transport_data *writer_data; /**< Where the writer data is
								saved. */
	struct ff_transport_data *reader_data; /**< Where the reader data is
								saved. */
	firefly_channel_rejected_f on_chan_rejected; /**< Callback called if this
												   channel could not be opened
												   due to remote node rejected
												   it. */
};

/**
 * Initializes a connection with protocol specific stuff.
 *
 * @param on_channel_opened Callback for when a channel has been opened.
 * @param on_channel_closed Callback for when a channel has been closed.
 * @param on_channel_recv Callback for when a channel has been recveived.
 * @param event_queue The event queue all events relating to this connection is
 * offered to.
 *
 * @return A new firefly_connection.
 * @retval NULL on error.
 */
struct firefly_connection *firefly_connection_new(
		firefly_channel_is_open_f on_channel_opened,
		firefly_channel_closed_f on_channel_closed,
		firefly_channel_accept_f on_channel_recv,
		transport_write_f transport_write,
		struct firefly_event_queue *event_queue, void *plat_spec,
		transport_connection_free plat_spec_free);

struct firefly_connection *firefly_connection_new_register(
		firefly_channel_is_open_f on_channel_opened,
		firefly_channel_closed_f on_channel_closed,
		firefly_channel_accept_f on_channel_recv,
		transport_write_f transport_write,
		struct firefly_event_queue *event_queue, void *plat_spec,
		transport_connection_free plat_spec_free, bool reg);

/*
 * The application must make sure the connection is no longer used after calling
 * this function. It is not thread safe to continue to use a connection after it
 * is closed. Hence the application must make use of the event queue,
 * mutexes/locks or any other feature preventing concurrency.
 */
void firefly_connection_close(struct firefly_connection *conn);

/**
 *
 * This function will call the trasnport connection free callback if provided.
 * The transport layer is responsible for preventing future use of the
 * connection and prevent any concurrent use from multiple threads using the
 * transport layer. This means the transport layer must either make use of the
 * event queue, use mutexes/locks or any other form of preventing concurrency.
 */
int firefly_connection_close_event(void *event_arg);

/**
 * @brief Frees a connection.
 *
 * @param conn The connection to free.
 */
void firefly_connection_free(struct firefly_connection **conn);

int firefly_connection_free_event(void *event_arg);

/**
 * @brief The function called by the transport layer upon received data.
 *
 * @param conn The connection the data is associated with.
 * @param data The received data.
 * @param size The size of the received data.
 */
void protocol_data_received(struct firefly_connection *conn,
							unsigned char *data, size_t size);

/**
 * @brief Create a new channel with some defaults.
 *
 * @param conn The connection to create the channel on.
 * @return The new channel.
 * @retval NULL If the function failed to allocate the new channel.
 */
struct firefly_channel *firefly_channel_new(struct firefly_connection *conn);

/**
 * @brief Free the supplied channel.
 *
 * @param chan The channel to free.
 * @param conn The connection the channel lives on.
 */
void firefly_channel_free(struct firefly_channel *chan);

/**
 * @brief The event argument of firefly_channel_open_event.
 */
struct firefly_event_chan_open {
	struct firefly_connection *conn; /**< The connection the channel is
						opened on. */
	firefly_channel_rejected_f rejected_cb; /**< The callback called if the
							request was rejected by
							the remote node. */
};

/**
 * @brief Open a channel on the connection. This is ment to be run in an event.
 *
 * A firefly_protocol_channel_open packet is sent with a reserved local id. The
 * firefly_channel struct is alocated and added to the chan_list of the
 * firefly_connection.
 *
 * @pram event_arg A firefly_event_chan_open.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 */
int firefly_channel_open_event(void *event_arg);

/**
 * @brief Free's and removes the firefly_channel from the firefly_connection.
 * This is ment to be run in an event.
 *
 * The firefly_channel is removed from the chan_list of the firefly_connection
 * and free'd.
 *
 * @param event_arg The channel to be free'd.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 */
int firefly_channel_closed_event(void *event_arg);

/**
 * @brief The event argument of firefly_channel_close_event.
 */
struct firefly_event_chan_close {
	struct firefly_connection *conn; /**< The connection to send the packet
						on. */
	firefly_protocol_channel_close chan_close; /**< The packet to send,
							must be correctly
							initialized with id's.
							*/
};

/**
 * @brief Sends a \c #firefly_protocol_channel_close and pushes an event to free
 * and remove the channel from its firefly_connection.
 *
 * @pram event_arg A firefly_event_chan_close.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 */
int firefly_channel_close_event(void *event_arg);

/**
 * @brief The callback registered with LabComm used to receive channel request.
 *
 * @param chan_req The decoded channel request
 * @param context The connection associated with the channel request.
 */
void handle_channel_request(firefly_protocol_channel_request *chan_req,
		void *context);

/**
 * @brief The event argument of handle_channel_request_event.
 */
struct firefly_event_chan_req_recv {
	struct firefly_connection *conn; /**< The connection the request was
						received on. */
	firefly_protocol_channel_request chan_req; /**< The received request.*/
};

/**
 * @brief Parses the firefly_protocol_channel_request. Calls the
 * firefly_channel_accept_f of the firefly_connection if the request is valid.
 *
 * The request is parsed and a firefly_channel struct is allocated. The
 * firefly_channel_accept_f is called, if it returns true a
 * firefly_protocol_channel_response is sent acknowledging the request.
 *
 * @param event_arg A firefly_event_chan_req_recv.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 */
int handle_channel_request_event(void *event_arg);

/**
 * @brief The callback registered with LabComm used to receive channel response.
 *
 * @param chan_res The decoded channel response.
 * @param context The connection associated with the channel response.
 */
void handle_channel_response(firefly_protocol_channel_response *chan_res,
		void *context);

/**
 * @brief The event argument of handle_channel_response_event.
 */
struct firefly_event_chan_res_recv {
	struct firefly_connection *conn; /**< The connection the request was
						received on. */
	firefly_protocol_channel_response chan_res; /**< The received
							response. */
};

/**
 * @brief The firefly_protocol_channel_repsonse is parsed and if valid the
 * firefly_channel_is_open_f is called and a firefly_protocol_channel_ack is
 * sent.
 *
 * @param event_arg A firefly_event_chan_res_recv.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 */
int handle_channel_response_event(void *event_arg);

/**
 * @brief The callback registered with LabComm used to receive channel ack.
 *
 * @param chan_res The decoded channel ack.
 * @param context The connection associated with the channel ack.
 */
void handle_channel_ack(firefly_protocol_channel_ack *chan_ack, void *context);

/**
 * @brief The event argument of handle_channel_ack_event.
 */
struct firefly_event_chan_ack_recv {
	struct firefly_connection *conn; /**< The connection the ack was
						received on. */
	firefly_protocol_channel_ack chan_ack; /**< The received ack. */
};

/**
 * @brief The firefly_protocol_channel_ack is parsed and if valid the
 * firefly_channel_is_open_f is called.
 *
 * @param event_arg A firefly_event_chan_ack_recv.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 */
int handle_channel_ack_event(void *event_arg);

/**
 * @brief The callback registered with LabComm used to receive channel close.
 *
 * @param chan_close The decoded channel close.
 * @param context The connection associated with the channel close.
 */
void handle_channel_close(firefly_protocol_channel_close *chan_close,
		void *context);

/**
 * @brief The callback registered with LabComm used to receive data sample.
 *
 * @param data The decoded data sample.
 * @param context The connection associated with the received data.
 */
void handle_data_sample(firefly_protocol_data_sample *data,
		void *context);

/**
 * @brief The event argument of handle_channel_ack_event.
 */
struct firefly_event_recv_sample {
	struct firefly_connection *conn; /**< The connection to send the sample
						on. */
	firefly_protocol_data_sample data; /**< The sample to send. */
};

/**
 * @brief The firefly_protocol_data_sample is parsed and if valid the app data is
 * decoded.
 *
 * @param event_arg A firefly_event_recv_sample.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 */
int handle_data_sample_event(void *event_arg);

/**
 * @brief The event argument of handle_channel_ack_event.
 */
struct firefly_event_send_sample {
	struct firefly_connection *conn; /**< The connection to send the sample
						on. */
	firefly_protocol_data_sample data; /**< The sample to send. */
};

/**
 * @brief Encodes and sends a firefly_protocol_data_sample on the firefly_connection.
 *
 * @param event_arg A firefly_event_recv_sample.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 */
int send_data_sample_event(void *event_arg);

/**
 * @brief Find and return the channel associated with the given connection with
 * the given local channel id.
 *
 * @param id The local ID of the channel.
 * @param conn The connection the channel is associated with.
 */
struct firefly_channel *find_channel_by_local_id(
		struct firefly_connection *conn, int id);

/**
 * @brief Add the channel to the connection.
 *
 * @param chan The channel to add.
 * @param conn The connection to add the channel to.
 */
void add_channel_to_connection(struct firefly_channel *chan,
		struct firefly_connection *conn);

/**
 * @brief Remove the channel from the connection.
 *
 * @param chan The channel to remove.
 * @param conn The connection to remove the channel from.
 */
struct firefly_channel *remove_channel_from_connection(struct firefly_channel *chan,
		struct firefly_connection *conn);

/**
 * @brief Generates new uniqe channel ID for the supplied firefly_connection.
 *
 * @param conn The connection the new channel ID is generated for.
 * @return The new uniqe channel ID.
 */
int next_channel_id(struct firefly_connection *conn);

/**
 * @brief A general LabComm reader used to feed a decoder with data from
 * memory.
 *
 * @param r The labcomm reader that requests data.
 * @param action What action is requested.
 * @param reader_data The data struct containing the encoded data.
 * @return Value indicating how the action could be handled.
 */
int firefly_labcomm_reader(labcomm_reader_t *r, labcomm_reader_action_t action,
		struct ff_transport_data *reader_data);

/**
 * @brief Feeds LabComm decoder with bytes from the transport layer.
 * @param r The labcomm reader that requests data.
 * @param action What action is requested.
 * @return Value indicating how the action could be handled.
 * TODO rename this. prefix ff_ is nonstandard. No need to prefix with firefly for private functions. probalby prefix "labcomm_" should be neought so we know that it has with labcomm to do.
 */
int ff_transport_reader(labcomm_reader_t *r, labcomm_reader_action_t action);

/**
 * @brief Write bytes that LabComm has encoded to a memory and is then sent to
 * the transport layer.
 * @param w The labcomm writer that encodes the data.
 * @param action What action is requested.
 * @return Value indicating how the action could be handled.
 * TODO rename this. prefix ff_ is nonstandard. No need to prefix with firefly for private functions. probalby prefix "labcomm_" should be neought so we know that it has with labcomm to do.
 */
int ff_transport_writer(labcomm_writer_t *w, labcomm_writer_action_t action);

/**
 * @brief Feeds LabComm decoder with bytes from the protocol layer.
 * @param r The labcomm reader that requests data.
 * @param action What action is requested.
 * @return Value indicating how the action could be handled.
 */
int protocol_reader(labcomm_reader_t *r, labcomm_reader_action_t action);

/**
 * @brief Encode application specific LabComm samples.
 * @param w The labcomm writer that encodes the data.
 * @param action What action is requested.
 * @return Value indicating how the action could be handled.
 */
int protocol_writer(labcomm_writer_t *w, labcomm_writer_action_t action);

#endif
