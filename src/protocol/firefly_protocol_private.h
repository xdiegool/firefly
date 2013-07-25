#ifndef FIREFLY_PROTOCOL_PRIVATE_H
#define FIREFLY_PROTOCOL_PRIVATE_H

#include <labcomm.h>
#include <signal.h>

#include <labcomm_ioctl.h>
#include <protocol/firefly_protocol.h>
#include <gen/firefly_protocol.h>

#include "utils/firefly_event_queue.h"

#define CHANNEL_ID_NOT_SET		(-1)
#define BUFFER_SIZE			(128)
#define FIREFLY_CONNECTION_CLOSED	(0)
#define FIREFLY_CONNECTION_OPEN		(1)

#ifndef FIREFLY_MALLOC
	#define FIREFLY_MALLOC(size) malloc(size)
#else
void *FIREFLY_MALLOC(size_t s);
#endif

#ifndef FIREFLY_FREE
	#define FIREFLY_FREE(ptr) free(ptr)
#else
void FIREFLY_FREE(void *ptr);
#endif

#define FIREFLY_RUNTIME_MALLOC(conn, size)					\
	(conn)->memory_replacements.alloc_replacement != NULL ?			\
		(conn)->memory_replacements.alloc_replacement(conn, size) :	\
		FIREFLY_MALLOC(size)

#define FIREFLY_RUNTIME_FREE(conn, ptr)						\
	(conn)->memory_replacements.free_replacement != NULL ?			\
		(conn)->memory_replacements.free_replacement(conn, ptr) : 	\
		FIREFLY_FREE(ptr)

#define FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER					\
  LABCOMM_IOW('f', 0, void*)

#define FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID				\
  LABCOMM_IOW('f', 1, unsigned char*)

void reg_proto_sigs(struct labcomm_encoder *enc,
		    struct labcomm_decoder *dec,
		    struct firefly_connection *conn);

typedef void (* protocol_data_received_f)(struct firefly_connection *conn,
					  unsigned char *data,
					  size_t size);

/**
 * @brief A structure for representing a node in a linked list of channels.
 */
struct channel_list_node {
	struct channel_list_node *next;	/**< A pointer to the next list node. */
	struct firefly_channel *chan;	/**< A pointer the the channel struct for this node. */
};

/**
 * @brief Write data on the specified connection
 * This is an interface implemented by all transport layers.
 *
 * @param data The data to be written.
 * @param data_size The size of the data to be written.
 * @param conn The #firefly_connection to write the data to.
 * @param important If true the packet must be resent until acked.
 * @param id If important is true the id is a return value and will contain the
 * identifier of the packet which must be used when acking the packet.
 */
typedef void (* firefly_transport_connection_write_f)(unsigned char *data, size_t data_size,
					struct firefly_connection *conn, bool important,
					unsigned char *id);

/**
 * @brief Inform transport that a packet is acked or should not be resent
 * anymore.
 *
 * @param pkg_id The id of the packet.
 * @param conn The #firefly_connection the packet is sent on.
 */
typedef void (* firefly_transport_connection_ack_f)(unsigned char pkg_id,
					struct firefly_connection *conn);

/**
 * @brief TODO
 */
typedef int (*firefly_transport_connection_open_f)
	(struct firefly_connection *conn);

/**
 * @brief TODO
 */
typedef int (*firefly_transport_connection_close_f)
	(struct firefly_connection *conn);

struct firefly_transport_connection {
	firefly_transport_connection_open_f open;/**< TODO */
	firefly_transport_connection_close_f close;/**< TODO */
	firefly_transport_connection_write_f write;/**< TODO */
	firefly_transport_connection_ack_f ack;/**< Inform transport that a packet
											 is acked or should not be resent
											 anymore. */
	void *context;/**< TODO */
};

/**
 * @brief A structure representing a connection.
 */
struct firefly_connection {
	sig_atomic_t				open;				/**< State of the
													  connection. */
	struct labcomm_encoder			*transport_encoder;		/**< The transport
															  layer encoder for
															  this connection.
															  */
	struct labcomm_decoder			*transport_decoder;		/**< The transport layer decoder for this connection. */
	struct channel_list_node		*chan_list;			/**< The list of channels associated with this connection. */
	struct firefly_event_queue		*event_queue;			/**< The queue to which spawned events are added. */
	int 					channel_id_counter;		/**< The unique id reserved to the next opened channel on the connection. */
	struct firefly_memory_funcs		memory_replacements;
	void					*context;			/**< A reference to an optional, user defined context.  */
	struct firefly_connection_actions 	*actions;			/**< Callbacks to the applicaiton. */
	struct firefly_transport_connection *transport;	/**< Transport specific connection data. */
};

/**
 * @breif A simple queue of important packets.
 */
struct firefly_channel_important_queue {
	struct firefly_event_send_sample *fess;
	struct firefly_channel_important_queue *next;
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
	struct firefly_channel_important_queue *important_queue; /**< The
	queue used to queue important packets when sending another. */

	unsigned char important_id; /**< The identifier used to reference the packet
								  to the transport layer. If 0 no packet is
								  resent. */
	int current_seqno; /**< The sequence number of the currently or last
						 important packet. */
	int remote_seqno; /**< The sequence number of the last received important
						packet. */
	struct labcomm_encoder *proto_encoder; /**< LabComm encoder for this
					   			channel.*/
	struct labcomm_decoder *proto_decoder; /**< LabComm decoder for this
					   			channel. */
	bool restricted_local;		/**< Neg. initiated locally.   */
	bool restricted_remote;	/**< Neg. initiated remotely.  */
};
struct labcomm_writer *protocol_labcomm_writer_new(
		struct firefly_channel *chan);
struct labcomm_writer *transport_labcomm_writer_new(
		struct firefly_connection *conn);
struct labcomm_reader *transport_labcomm_reader_new(
		struct firefly_connection *conn);
struct labcomm_reader *protocol_labcomm_reader_new(
		struct firefly_connection *conn);
void protocol_labcomm_reader_free(struct labcomm_reader *r);
void transport_labcomm_reader_free(struct labcomm_reader *r);
void protocol_labcomm_writer_free(struct labcomm_writer *w);
void transport_labcomm_writer_free(struct labcomm_writer *w);

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
 * @brief TODO
 */
struct firefly_connection *firefly_connection_new(
		struct firefly_connection_actions *actions,
		struct firefly_memory_funcs *memory_replacements,
		struct firefly_event_queue *event_queue,
		struct firefly_transport_connection *tc);

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
 *
 */
//TODO comments
void handle_ack(firefly_protocol_ack *ack, void *context);

struct firefly_event_channel_restrict_request {
	struct firefly_connection *conn; /**< The connection the request was
						received on. */
	firefly_protocol_channel_restrict_request rreq; /**< The request.*/
};

void handle_channel_restrict_request(
		firefly_protocol_channel_restrict_request *data,
		void *context);

int channel_restrict_request_event(void *context);

struct firefly_event_chan_restrict_ack {
	struct firefly_connection *conn; /**< The connection the request was
						received on. */
	firefly_protocol_channel_restrict_request rack; /**< The request.*/
};

void handle_channel_restrict_ack(
		firefly_protocol_channel_restrict_ack *data,
		void *context);

int channel_restrict_ack_event(void *context);

/**
 * @brief The event argument of handle_channel_ack_event.
 */
struct firefly_event_send_sample {
	struct firefly_channel *chan; /**< The channel to send the sample on. */
	firefly_protocol_data_sample data; /**< The sample to send. */
	unsigned char *important_id;
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
 * @brief Gets and updates the sequence number used to identify important
 * packets.
 *
 * @param chan The concerned channel.
 * @return The next sequence number.
 */
int firefly_channel_next_seqno(struct firefly_channel *chan);

/**
 * @brief The event couterpart to the user accessible function.
 * @param earg The channel.
 */
int firefly_channel_restrict_event(void *earg);

/**
 * @brief The event couterpart to the user accessible function.
 * @param earg The channel.
 */
int firefly_channel_unrestrict_event(void *earg);

/**
 * @brief TODO
 */
void firefly_channel_ack(struct firefly_channel *chan);
#endif
