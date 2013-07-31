#ifndef FIREFLY_TRANSPORT_ETH_STELLARIS_PRIVATE_H
#define FIREFLY_TRANSPORT_ETH_STELLARIS_PRIVATE_H

#include <transport/firefly_transport.h>
#include "transport/firefly_transport_private.h"
#include <transport/firefly_transport_eth_stellaris.h>
#include <utils/firefly_event_queue.h>

#include <signal.h>

/**
 * @brief The protocol specified in every firefly packet. This is used to filter
 * out ethernet packets of value.
 */
#define FIREFLY_ETH_PROTOCOL        (0x1337)
/**
 * @defgroup eth_pkt Ethernet packet definitions
 * @brief Definitions used to navigate an ethernet packet.
 */
/**
 * @brief The max length of the data block in an ethernet packet.
 * @ingroup eth_pkt
 */
#define STELLARIS_ETH_MAX_DATA_LEN  (800)
/**
 * @brief The length of the address field in an ethernet packet header.
 * @ingroup eth_pkt
 */
#define ETH_ADDR_LEN                (6)
/**
 * @brief The length of the type field in an ethernet packet header.
 * @ingroup eth_pkt
 */
#define ETH_TYPE_LEN                (2)
/**
 * @brief The length of the header block in an ethernet packet.
 * @ingroup eth_pkt
 */
#define ETH_HEADER_LEN              (ETH_TYPE_LEN + ETH_ADDR_LEN * 2)
/**
 * @brief The max length of an entire ethernet packet.
 * @ingroup eth_pkt
 */
#define STELLARIS_ETH_MAX_FRAME_LEN \
		(STELLARIS_ETH_MAX_DATA_LEN + ETH_HEADER_LEN)

/**
 * @brief The offset of the destination addres in an ethernet packet.
 * @ingroup eth_pkt
 */
#define ETH_DST_OFFSET   (0)
/**
 * @brief The offset of the source addres in an ethernet packet.
 * @ingroup eth_pkt
 */
#define ETH_SRC_OFFSET   (ETH_DST_OFFSET + ETH_ADDR_LEN)
/**
 * @brief The offset of the type field in an ethernet packet.
 * @ingroup eth_pkt
 */
#define ETH_PROTO_OFFSET (ETH_SRC_OFFSET + ETH_ADDR_LEN)
/**
 * @brief The offset of the data block in an ethernet packet.
 * @ingroup eth_pkt
 */
#define ETH_DATA_OFFSET  (ETH_PROTO_OFFSET + ETH_TYPE_LEN)

/**
 * @brief The data structure with specific data needed by the transport layer.
 */
struct transport_llp_eth_stellaris {
	unsigned char src_addr[ETH_ADDR_LEN]; /**< The address of this node as sent
											on the network. */
	firefly_on_conn_recv_eth_stellaris on_conn_recv; /**< The callback to be
													 called when a new
													 connection is received. */
	struct firefly_event_queue *event_queue; /**< The event queue to push new
											   events to. */
};

/**
 * @brief The data structure with specific connection data for ethernet
 * stellaris.
 */
struct firefly_transport_connection_eth_stellaris {
	unsigned char remote_addr[ETH_ADDR_LEN]; /**< The address of the remote node
											   as sent on the network. */
	struct firefly_transport_llp *llp; /**< The \a llp this connection is
										 associated with. */
};

/**
 * @brief The argument type of a read event.
 */
struct firefly_event_llp_read_eth_stellaris {
	struct firefly_transport_llp *llp; /**< The llp the data was read on. */
	unsigned char *eth_packet; /**< The entire received ethernet packet. */
	long len; /**< The length of the ethernet packet. */
};

/**
 * @brief Helper function to get the protocol from an Ethernet frame.
 *
 * @param frame The Ethernet frame to extract the protocol field from.
 *
 * @return The protocol.
 */
short get_ethernet_proto(unsigned char *frame);

/**
 * @brief Helper function to print a MAC-address to a buffer.
 *
 * @param buf The buffer to write the address to. It will be null terminated.
 * @param addr The MAC-address to convert to a string.
 */
void sprint_mac(char *buf, unsigned char *addr);

/**
 * @brief Helper function to build Ethernet frames.
 *
 * Builds an Ethernet frame that can be sent with send_ethernet_frame().
 *
 * @warning The caller must \c free the memory returned from this function to
 * avoid memory leaks.
 *
 * @param src The source Ethernet address.
 * @param dest The destination Ethernet address.
 * @param data The data to send.
 * @param data_len The length of the data to send.
 *
 * @return The constructed Ethernet frame.
 * @retval A new unsigned char pointer to a buffer containing the frame.
 * @retval NULL on failuer.
 * @see #send_ethernet_frame()
 */
unsigned char *build_ethernet_frame(unsigned char *src, unsigned char *dest,
		unsigned char *data, size_t data_len);

/**
 * @brief Helper function to send an Ethernet frame.
 *
 * Sends an ethernet frame as created by #build_ethernet_frame() on the Ethernet
 * interface specified by the llp. Free's the ethframe when done sending.
 *
 * @param frame The Ethernet frame to send.
 * @param frame_len The length of the frame to send.
 *
 * @return An \c int indicating the result.
 * @retval 0 on success
 * @retval -1 on failure.
 */
int send_ethernet_frame(unsigned char *frame, long frame_len);

/**
 * @brief The event callback executing a free event.
 *
 * Will close all connections before freeing the llp. It only frees the llp if
 * there were no connections left to close, if there are any connections, it
 * will call close on each of them and then create a new free event.
 *
 * @param event_arg The event arguments, will be cast to a \c struct
 * #firefly_transport_llp.
 *
 * @return An \c int indicating the success of the event.
 * @retval Currently only 0 is returned for success.
 */
int firefly_transport_llp_eth_stellaris_free_event(void *event_arg);

/**
 * @brief Sends a new data frame on the Stellaris interface.
 *
 * This function takes the provided data and connection and creates an Ethernet
 * frame which it sends on the Ethernet interface. The source and destination
 * addresses are taken from the provided connection.
 *
 * @param data The data to send.
 * @param data_size The size of the data to send.
 * @param conn The connection from which the source and destination addresses
 * are taken from.
 * @param important If true the packet will be resent until it is acked by
 * calling #firefly_transport_eth_stellaris_ack or max retries is reached.
 * @param id The variable to save the resend packed id in.
 * @see #firefly_transport_connection_write_f()
 * @see #firefly_transport_eth_stellaris_ack()
 */
void firefly_transport_eth_stellaris_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id);

/**
 * @brief Ack an important packed. Removes the packet from the resend queue.
 * Implements #firefly_transport_connection_ack_f()
 *
 * @param pkt_id The id previously set by
 * #firefly_transport_eth_stellaris_write.
 * @param conn The connection the packet was sent on.
 * @see #firefly_transport_connection_ack_f()
 * @see #firefly_transport_eth_posix_write()
 */
void firefly_transport_eth_stellaris_ack(unsigned char pkt_id,
		struct firefly_connection *conn);

/**
 * @brief The event callback executing a read.
 *
 * @param event_args The event arguments, will be cast to a \c struct
 * #firefly_event_llp_read_eth_stellaris.
 *
 * @return An \c int indicating the success of the event.
 * @retval Currently only 0 is returned for success.
 * @see #firefly_transport_eth_stellaris_read()
 */
int firefly_transport_eth_stellaris_read_event(void *event_args);

#endif
