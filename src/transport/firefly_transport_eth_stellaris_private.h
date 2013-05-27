#ifndef FIREFLY_TRANSPORT_ETH_STELLARIS_PRIVATE_H
#define FIREFLY_TRANSPORT_ETH_STELLARIS_PRIVATE_H

#include <transport/firefly_transport.h>
#include "transport/firefly_transport_private.h"
#include <transport/firefly_transport_eth_stellaris.h>
#include <utils/firefly_event_queue.h>

#include <signal.h>

#define FIREFLY_ETH_PROTOCOL        (0x1337)
#define STELLARIS_ETH_MAX_DATA_LEN  (800)
#define ETH_HEADER_LEN              (14)
#define STELLARIS_ETH_MAX_FRAME_LEN (814)
#define ETH_ADDR_LEN                (6)

// Offset for the different fields in an Ethernet frame.
#define ETH_DST_OFFSET   (0)
#define ETH_SRC_OFFSET   (ETH_DST_OFFSET + ETH_ADDR_LEN)
#define ETH_PROTO_OFFSET (ETH_SRC_OFFSET + ETH_ADDR_LEN)
#define ETH_DATA_OFFSET  (ETH_PROTO_OFFSET + 2)

/**
 * The data structure with specific data needed by the transport layer.
 */
struct transport_llp_eth_stellaris {
	unsigned char src_addr[ETH_ADDR_LEN];
	firefly_on_conn_recv_eth_stellaris on_conn_recv;
	struct firefly_event_queue *event_queue;
};

/**
 * The data structure with specific data for the protocol layer.
 */
struct protocol_connection_eth_stellaris {
	unsigned char remote_addr[ETH_ADDR_LEN];
	struct firefly_transport_llp *llp;
};

/**
 * The data structure for the read event.
 */
struct firefly_event_llp_read_eth_stellaris {
	struct firefly_transport_llp *llp;
	unsigned char *eth_packet;
	long len;
};

/**
 * Helper function to get the protocol from an Ethernet frame.
 *
 * @param frame The Ethernet frame to extract the protocol field from.
 *
 * @return The protocol.
 */
short get_ethernet_proto(unsigned char *frame);

/**
 * @brief Helper function to print a MAC-address to a buffer.
 *
 * @param buf The buffer to print to address to. It will be null terminated
 * when done.
 * @param addr The MAC-address to print.
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
 * @retval A new unsigned char pointer to a buffer containing the frame or \c
 * NULL on failure.
 */
unsigned char *build_ethernet_frame(unsigned char *src, unsigned char *dest,
		unsigned char *data, size_t data_len);

/**
 * @brief Helper function to send an Ethernet frame.
 *
 * Sends the frame (created by build_ethernet_frame()) on the Ethernet interface
 * specified by llp->ethernet_base. Free's the ethframe when done sending.
 *
 * @param llp The llp to send on,
 * @param frame The Ethernet frame to send.
 *
 * @return An \c int representing the success.
 * @retval 0 on success, -1 on failure.
 */
int send_ethernet_frame(unsigned char *frame, long frame_len);

/**
 * @brief The event callback executing a free event.
 *
 * Will close all connections before freeing the llp. It only frees the llp if
 * there were no connections left to close, if there are any connections, it
 * will call close on each of them and then create a new free event.
 *
 * @param event_args The event arguments, will be cast to a struct
 * firefly_transport_llp.
 *
 * @return An \c int indicating the success of the event.
 * @retval Currently only 0 is returned for success.
 */
int firefly_transport_llp_eth_stellaris_free_event(void *event_arg);

/**
 * @brief Frees the Stellaris specific part of the provided connection.
 *
 * Also removes the connection from the connection list on the llp.
 *
 * @param conn The connection to free the Stellaris specific part of.
 */
void firefly_transport_connection_eth_stellaris_free(
		struct firefly_connection *conn);

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
 */
void firefly_transport_eth_stellaris_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id);

void firefly_transport_eth_stellaris_ack(unsigned char pkg_id,
		struct firefly_connection *conn);

/**
 * @brief The event callback executing a read.
 *
 * @param event_args The event arguments, will be cast to a struct
 * firefly_event_llp_read_eth_stellaris.
 *
 * @return An \c int indicating the success of the event.
 * @retval Currently only 0 is returned for success.
 */
int firefly_transport_eth_stellaris_read_event(void *event_args);

#endif
