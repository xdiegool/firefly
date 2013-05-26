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

#define ETH_DST_OFFSET   (0)
#define ETH_SRC_OFFSET   (ETH_DST_OFFSET + ETH_ADDR_LEN)
#define ETH_PROTO_OFFSET (ETH_SRC_OFFSET + ETH_ADDR_LEN)
#define ETH_DATA_OFFSET  (ETH_PROTO_OFFSET + 2)

struct transport_llp_eth_stellaris {
	unsigned char src_addr[ETH_ADDR_LEN];
	firefly_on_conn_recv_eth_stellaris on_conn_recv;
	struct firefly_event_queue *event_queue;
};

struct protocol_connection_eth_stellaris {
	unsigned char remote_addr[ETH_ADDR_LEN];
	struct firefly_transport_llp *llp;
};

struct firefly_event_llp_read_eth_stellaris {
	struct firefly_transport_llp *llp;
	unsigned char *eth_packet;
	long len;
};

/**
 * Helper function to get the protocol from an ethernet frame.
 *
 * @param frame The Ethernet frame to extract the protocol field from.
 *
 * @return The protocol.
 */
short get_ethernet_proto(unsigned char *frame);

/**
 * Helper function to build ethernet frames.
 *
 * @param src The source address.
 * @param dest The destination address.
 * @param data The data to send.
 * @param data_len The length of the data.
 *
 * @return The constructed ethernet frame.
 * @retval struct eth_frame or \c NULL on failure.
 */
unsigned char *build_ethernet_frame(unsigned char *src, unsigned char *dest,
		unsigned char *data, size_t data_len);

/**
 * Helper function to send an Ethernet frame on the Ethernet interface
 * specified by llp->ethernet_base. Free's the ethframe when done sending.
 *
 * @param llp The llp to send on,
 * @param frame The Ethernet frame to send.
 *
 * @return An \c int representing the success.
 * @retval 0 on success, -1 on failure.
 */
int send_ethernet_frame(unsigned char *frame, long frame_len);

int firefly_transport_eth_stellaris_read_event(void *event_args);

int firefly_transport_llp_eth_stellaris_free_event(void *event_arg);

void firefly_transport_connection_eth_stellaris_free(
		struct firefly_connection *conn);

void firefly_transport_eth_stellaris_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id);

void firefly_transport_eth_stellaris_ack(unsigned char pkg_id,
		struct firefly_connection *conn);

#endif
