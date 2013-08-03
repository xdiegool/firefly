#ifndef FIREFLY_PROTOCOL_PRIVATE_H
#define FIREFLY_PROTOCOL_PRIVATE_H

#include <labcomm.h>
#include <signal.h>

#include <labcomm_ioctl.h>
#include <protocol/firefly_protocol.h>
#include <gen/firefly_protocol.h>

#include "utils/firefly_event_queue.h"

/**
 * @brief A channel ID indicating that no channel ID has been set yet.
 */
#define CHANNEL_ID_NOT_SET		(-1)

/**
 * @brief The size of the writer buffer used by Labcomm.
 */
#define BUFFER_SIZE			(128)

/**
 * @defgroup conn_state Connection State Values
 * @brief The different values the state of a connection may have.
 */
/**
 * @brief Defines the opened state of a connection.
 * @ingroup conn_state
 */
#define FIREFLY_CONNECTION_OPEN (1)
/**
 * @brief Defines the closed state of a connection.
 * @ingroup conn_state
 */
#define FIREFLY_CONNECTION_CLOSED (0)

/**
 * @brief The priority of the connection close event.
 */
#define FIREFLY_CONNECTION_CLOSE_PRIORITY FIREFLY_PRIORITY_MEDIUM

/**
 * @brief A macro that, if defined, lets the user replace malloc() with
 * their own version.
 *
 * If the macro is not defined, the regular malloc() function is used.
 */
#ifndef FIREFLY_MALLOC
	#define FIREFLY_MALLOC(size) malloc(size)
#else
void *FIREFLY_MALLOC(size_t s);
#endif

/**
 * @brief A macro that, if defined, lets the user replace free() with
 * their own version.
 *
 * If the macro is not defined, the regular free() function is used.
 */
#ifndef FIREFLY_FREE
	#define FIREFLY_FREE(ptr) free(ptr)
#else
void FIREFLY_FREE(void *ptr);
#endif

/**
 * @brief A macro that should be used for any malloc's done when in
 * runtime (i.e. not when setting up or tearing down stuff).
 *
 * If the user has not set the alloc_replacement function pointer, we
 * fall back to using the #FIREFLY_MALLOC() macro above.
 *
 * @param conn The connection the malloc is performed for.
 * @param size The size of the data block to be allocated.
 *
 * @return A pointer to the data block or NULL on failure.
 * @retval NULL If there was an error allocating memory.
 */
#define FIREFLY_RUNTIME_MALLOC(conn, size)					\
	(conn)->memory_replacements.alloc_replacement != NULL ?			\
		(conn)->memory_replacements.alloc_replacement(conn, size) :	\
		FIREFLY_MALLOC(size)

/**
 * @brief A macro that should be used to free any data allocated by
 * #FIREFLY_RUNTIME_MALLOC() when in runtime (i.e. not when setting up
 * or tearing down stuff).
 *
 * If the user has not set the free_replacement function pointer, we
 * fall back to using the #FIREFLY_FREE() macro above.
 *
 * @param conn The connection the allocation was performed on.
 * @param ptr The pointer to the data block to be freed.
 */
#define FIREFLY_RUNTIME_FREE(conn, ptr)						\
	(conn)->memory_replacements.free_replacement != NULL ?			\
		(conn)->memory_replacements.free_replacement(conn, ptr) : 	\
		FIREFLY_FREE(ptr)

/**
 * @brief A macro for setting the read buffer through Labcomm's ioctl
 * functionality.
 */
#define FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER					\
  LABCOMM_IOW('f', 0, void*)

/**
 * @brief A macro for informing that the next packet is important
 * through Labcomm's ioctl functionality.
 */
#define FIREFLY_LABCOMM_IOCTL_TRANS_SET_IMPORTANT_ID				\
  LABCOMM_IOW('f', 1, unsigned char*)

#define FF_ERRMSG_MAXLEN (128)

#define FIREFLY_CONNECTION_RAISE(conn, reason, msg) \
	do { \
		bool prop = conn->actions->connection_error ? \
			conn->actions->connection_error(conn, reason, msg) : false; \
		for (struct channel_list_node *n = conn->chan_list; n != NULL && prop; \
				n = n->next) { \
			if (conn->actions->channel_error) \
				conn->actions->channel_error(n->chan, reason, msg); \
		} \
	} while (false); \


/**
 * @brief Registers the protocol signatures that are needed by the
 * protocol.
 *
 * @param enc The encoder to register protocol signatures on.
 * @param dec The decoder to register protocol signatures on.
 * @param conn The connection on which the signatures should be
 * registered on.
 */
void reg_proto_sigs(struct labcomm_encoder *enc,
		    struct labcomm_decoder *dec,
		    struct firefly_connection *conn);

/**
 * @brief A prototype for the callback used by the transport layer to
 * pass received data to the protocol layer.
 *
 * @param conn The connection the data is received on.
 * @param data The data received.
 * @param size The size of the data buffer.
 */
typedef void (* protocol_data_received_f)(struct firefly_connection *conn,
					  unsigned char *data,
					  size_t size);

/**
 * @brief A structure for representing a node in a linked list of channels.
 */
struct channel_list_node {
	struct channel_list_node *next;	/**< A pointer to the next list node. */
	struct firefly_channel *chan;	/**< A pointer the channel struct for this node. */
};

/**
 * @brief A prototype for the function used to write data on the
 * specified connection.
 *
 * This is an interface implemented by all transport layers.
 *
 * @param data The data to be written.
 * @param data_size The size of the data to be written.
 * @param conn The #firefly_connection to write the data to.
 * @param important If true the packet must be re-sent until
 * acknowledged.
 * @param id If important is true the id is a return value and will
 * contain the identifier of the packet which must be used when
 * acknowleding the packet.
 */
typedef void (* firefly_transport_connection_write_f)(unsigned char *data, size_t data_size,
					struct firefly_connection *conn, bool important,
					unsigned char *id);

/**
 * @brief Inform transport that a packet is acknowledged and should not
 * be resent anymore.
 *
 * @param pkg_id The id of the packet.
 * @param conn The #firefly_connection the packet is sent on.
 */
typedef void (* firefly_transport_connection_ack_f)(unsigned char pkg_id,
					struct firefly_connection *conn);

/**
 * @brief A prototype of the function called from the protocol layer to
 * setup transport layer specific stuff of the given
 * #firefly_connection and add it to the connection list.
 *
 * @param conn The connection that is being opened.
 *
 * @return An int representing the success of the function.
 * @retval 0 On success.
 */
typedef int (*firefly_transport_connection_open_f)
	(struct firefly_connection *conn);

/**
 * @brief A prototype of the function called from the protocol layer to
 * free transport layer specific stuff and remove the connection from
 * the connection list.
 *
 * @param conn The connection to close and free.
 *
 * @return An int indicating the success or failure of the function.
 * @retval 0 On success.
 */
typedef int (*firefly_transport_connection_close_f)
	(struct firefly_connection *conn);

/**
 * @brief a data structure containing function pointers to functions
 * defined by the transport layer.
 */
struct firefly_transport_connection {
	firefly_transport_connection_open_f open;/**< Used when opening
											   connections, see
											   #firefly_transport_connection_open_f. */
	firefly_transport_connection_close_f close;/**< Used when closing
												 connections, see
												 #firefly_transport_connection_close_f. */
	firefly_transport_connection_write_f write;/**< Used when writing
												 data, see
												 #firefly_transport_connection_write_f. */
	firefly_transport_connection_ack_f ack;/**< Inform transport that a packet
											 is acked or should not be resent
											 anymore, see #firefly_transport_connection_ack_f. */
	void *context;/**< A context used to pass data to the functions,
					contains the platform specific
					transport_connection_* type.  */
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
	struct firefly_memory_funcs		memory_replacements; /**< A struct containing function
														   pointers to runtime memory
														   replacement functions. See
														   #firefly_memory_funcs, #FIREFLY_RUNTIME_MALLOC and
														   #FIREFLY_RUNTIME_FREE. */
	void					*context;			/**< A reference to an optional, user defined context.  */
	struct firefly_connection_actions 	*actions;			/**< Callbacks to the applicaiton. */
	struct firefly_transport_connection *transport;	/**< Transport specific connection data. */
};

/**
 * @brief A simple queue of important packets.
 */
struct firefly_channel_important_queue {
	struct firefly_event_send_sample *fess; /**< The data of this node. */
	struct firefly_channel_important_queue *next; /**< The pointer to the next
													element in the queue. */
};

/**
 * @brief An enum of the different states a channel can be in.
 */
enum firefly_channel_state {FIREFLY_CHANNEL_READY, FIREFLY_CHANNEL_OPEN};

/**
 * @brief A structure representing a channel.
 */
struct firefly_channel {
	struct firefly_connection *conn; /**< The connection the channel exists
									on. */
	int local_id; /**< The local ID used to identify this channel */
	int remote_id; /**< The ID used by the remote node to identify
				this channel */
	enum firefly_channel_state state; /**< The state of the channel */
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

/**
 * @brief Creates a new labcomm_writer for the provided channel
 * (protocol layer).
 *
 * @param chan The channel to create the writer for.
 *
 * @return A pointer to the newly created labcomm_writer.
 * @retval NULL On failure.
 */
struct labcomm_writer *protocol_labcomm_writer_new(
		struct firefly_channel *chan);

/**
 * @brief Creates a new labcomm_writer for the provided connection
 * (transport layer).
 *
 * @param conn The connection to create the writer for.
 *
 * @return A pointer to the newly created labcomm_writer.
 * @retval NULL On failure.
 */
struct labcomm_writer *transport_labcomm_writer_new(
		struct firefly_connection *conn);

/**
 * @brief Creates a new labcomm_reader for the provided connection
 * (protocol layer).
 *
 * @param conn The connection to create a reader for.
 *
 * @return A pointer to the newly created labcomm_reader or NULL.
 * @retval NULL On failure.
 */
struct labcomm_reader *protocol_labcomm_reader_new(
		struct firefly_connection *conn);

/**
 * @brief Creates a new labcomm_reader for the provided connection
 * (transport layer).
 *
 * @param conn The connection to create a reader for.
 *
 * @return A pointer to the newly created labcomm_reader or NULL.
 * @retval NULL On failure.
 */
struct labcomm_reader *transport_labcomm_reader_new(
		struct firefly_connection *conn);

/**
 * @brief Frees the provided labcomm_reader (protocol layer).
 *
 * @param r The labcomm_reader to free.
 */
void protocol_labcomm_reader_free(struct labcomm_reader *r);

/**
 * @brief Frees the provided labcomm_reader (transport layer).
 *
 * @param r The labcomm_reader to free.
 */
// TODO: Merge this with protocol_labcomm_reader_free since they do the
// same thing? It's only internal stuff anyways.
void transport_labcomm_reader_free(struct labcomm_reader *r);

/**
 * @brief Frees the provided labcomm_writer (protocol layer).
 *
 * @param w The labcomm_writer to free.
 */
void protocol_labcomm_writer_free(struct labcomm_writer *w);

/**
 * @brief Frees the provided labcomm_writer (transport layer).
 *
 * @param w The labcomm_writer to free.
 */
// TODO: Merge this with protocol_labcomm_writer_free since they do the
// same thing? It's only internal stuff anyways.
void transport_labcomm_writer_free(struct labcomm_writer *w);

/**
 * @brief The event that will be executed when closing a connection.
 *
 * @param event_arg The argument of the event, should be the
 * #firefly_connection that should be closed (otherwise, it'll be 'ard
 * to close it, won't it?).
 *
 * @return An int indicating the function's success or failure.
 * @retval 0 On success
 */
int firefly_connection_close_event(void *event_arg);

/**
 * @brief Creates a new #firefly_connection with the provided data.
 *
 * @param actions The action struct with callbacks to the application.
 * @param memory_replacements The struct containing memory replacement
 * functions.
 * @param event_queue The event queue.
 * @param tc The #firefly_transport_connection with transport layer
 * specific functions and data.
 *
 * @return The new #firefly_connection on success or NULL on failure.
 * @retval NULL On failure.
 */
struct firefly_connection *firefly_connection_new(
		struct firefly_connection_actions *actions,
		struct firefly_memory_funcs *memory_replacements,
		struct firefly_event_queue *event_queue,
		struct firefly_transport_connection *tc);

/**
 * @brief Frees a connection.
 *
 * The function first creates channel closed events for all channels and
 * then frees the channel list. Then it frees all other resources of the
 * connection.
 *
 * @param conn The connection to free. This will be set to NULL before
 * returning.
 */
void firefly_connection_free(struct firefly_connection **conn);

/**
 * @brief The event freeing a connection.
 *
 * @param event_arg The connection to free.
 *
 * @return An int indicating the success or failure of the function.
 * @retval 0 On success.
 */
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
 * @brief The event performing the opening of a channel on the
 * connection.
 *
 * @param event_arg A firefly_event_chan_open.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 * @see #firefly_channel_open
 */
int firefly_channel_open_event(void *event_arg);

/**
 * @brief Sets opened state on channel and calls on_open callback if  the
 * channel was not already open.
 *
 * @param chan The channel to set opened state on.
 */
void firefly_channel_internal_opened(struct firefly_channel *chan);

/**
 * @brief The event that frees and removes the firefly_channel.
 *
 * @param event_arg The channel to be freed.
 * @return Integer indicating the result of the event.
 * @retval Negative integer upon error.
 * @see #firefly_channel_close
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
 * @brief The event sending a close packet for the provided channel
 *
 * @param event_arg A firefly_event_chan_close.
 * @return Integer indicating the result of the event.
 * @retval Negative integer upon error.
 * @see #firefly_channel_close
 */
int firefly_channel_close_event(void *event_arg);

/**
 * @brief The callback registered with LabComm used to receive channel request.
 *
 * The function copies the data to a new buffer and creates an event
 * that will handle calling the correct callbacks to the application and
 * sending the correct response packet depending on whether the user
 * chooses to accept it or not.
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
 * @brief The event that parses the firefly_protocol_channel_request.
 *
 * @param event_arg A firefly_event_chan_req_recv.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 * @see #handle_channel_request
 */
int handle_channel_request_event(void *event_arg);

/**
 * @brief The callback registered with LabComm used to receive channel response.
 *
 * This function copies the data to a new buffer and creates a new event
 * that will handle the rest. The event finds the correct channel and
 * calls the correct callbacks to the application depending on the
 * response (ACK or NAK).
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
 * @brief The event that parses a firefly_protocol_channel_repsonse.
 *
 * @param event_arg A firefly_event_chan_res_recv.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 * @see #handle_channel_response
 */
int handle_channel_response_event(void *event_arg);

/**
 * @brief The callback registered with LabComm used to receive channel ack.
 *
 * This function copies the data into new buffers and creates a new
 * event handling the rest. The event will handle finding the correct
 * channel and marking a packet as acknowledged. It also calls the
 * correct callbacks to the application.
 *
 * @param chan_ack The decoded channel ack.
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
 * @brief The event that parses a firefly_protocol_channel_ack.
 *
 * @param event_arg A firefly_event_chan_ack_recv.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 * @see #handle_channel_ack
 */
int handle_channel_ack_event(void *event_arg);

/**
 * @brief The callback registered with LabComm used to receive channel close.
 *
 * This function creates an event that handles the actual closing.
 *
 * @param chan_close The decoded channel close.
 * @param context The connection associated with the channel close.
 */
void handle_channel_close(firefly_protocol_channel_close *chan_close,
		void *context);

/**
 * @brief The callback registered with LabComm used to receive data sample.
 *
 * This function creates an event that handles the data sample. The
 * event handles finding the correct channel, checking if the packet
 * should be acknowledged, and lastly decode the user data inside the
 * packet.
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
 * @brief The event that parses a firefly_protocol_data_sample.
 *
 * @param event_arg A firefly_event_recv_sample.
 * @return Integer idicating the resutlt of the event.
 * @retval Negative integer upon error.
 * @see #handle_data_sample
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
 * the given remote channel id.
 *
 * @param id The remote ID of the channel.
 * @param conn The connection the channel is associated with.
 */
struct firefly_channel *find_channel_by_remote_id(
		struct firefly_connection *conn, int id);
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
 * @brief Raise the specified error on the specified connection from the event
 * queue of the connection. Should only be called from outside events.
 *
 * @see #FIREFLY_CONNECTION_RAISE
 */
void firefly_connection_raise_later(struct firefly_connection *conn,
		enum firefly_error reason, const char *msg);

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
 * @brief Internal function which will ack to the transport layer any non-acked
 * important packet on the provided channel.
 *
 * @param chan All important packets of this channel will be acked.
 */
void firefly_channel_ack(struct firefly_channel *chan);
#endif
