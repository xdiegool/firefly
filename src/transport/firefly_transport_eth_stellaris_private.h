#ifndef FIREFLY_TRANSPORT_ETH_STELLARIS_PRIVATE_H
#define FIREFLY_TRANSPORT_ETH_STELLARIS_PRIVATE_H


#include <transport/firefly_transport.h>
#include "transport/firefly_transport_private.h"
#include <transport/firefly_transport_eth_stellaris.h>

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
	unsigned char *recv_buf;
	firefly_on_conn_recv_eth_stellaris on_conn_recv;
};

struct protocol_connection_eth_stellaris {
	unsigned char src_addr[ETH_ADDR_LEN];
	unsigned char remote_addr[ETH_ADDR_LEN];
	sig_atomic_t open; /**< The flag indicating the opened state of a
						 connection.*/
};

short get_ethernet_proto(unsigned char *frame) {
	short proto = 0;
	proto = frame[ETH_PROTO_OFFSET] << 8;
	proto |= frame[ETH_PROTO_OFFSET + 1];

	return proto;
}

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

int firefly_transport_connection_eth_stellaris_free_event(void *event_arg);

void firefly_transport_connection_eth_stellaris_free(
		struct firefly_connection *conn);

void firefly_transport_eth_stellaris_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn);

#endif
