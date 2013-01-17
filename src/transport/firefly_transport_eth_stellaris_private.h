#ifndef FIREFLY_TRANSPORT_ETH_STELLARIS_PRIVATE_H
#define FIREFLY_TRANSPORT_ETH_STELLARIS_PRIVATE_H


#include <transport/firefly_transport.h>
#include "transport/firefly_transport_private.h"
#include <transport/firefly_transport_eth_stellaris.h>

#include <signal.h>

#define FIREFLY_ETH_PROTOCOL	0x1337
#define STELLARIS_ETH_MAX_DATA_LEN (800)
#define ETH_HEADER_LEN (14)
#define STELLARIS_ETH_MAX_FRAME_LEN (814)
#define ETH_ADDR_LEN (6)

struct ethhdr {
	unsigned char dest[ETH_ADDR_LEN];
	unsigned char src[ETH_ADDR_LEN];
	unsigned short proto;
};

union ethframe {
	struct {
		struct ethhdr header;
		unsigned char data[STELLARIS_ETH_MAX_DATA_LEN];
	} field;
	unsigned char buffer[STELLARIS_ETH_MAX_FRAME_LEN];
};

struct transport_llp_eth_stellaris {
	unsigned char src_addr[ETH_ADDR_LEN];
	union ethframe recv_buf;
	firefly_on_conn_recv_eth_stellaris on_conn_recv;
};

struct protocol_connection_eth_stellaris {
	// TODO: Need src addr here!
	unsigned char remote_addr[ETH_ADDR_LEN];
	sig_atomic_t open; /**< The flag indicating the opened state of a
						 connection.*/
};

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
union ethframe *build_ethernet_frame(unsigned char *src, unsigned char *dest,
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
int send_ethernet_frame(union ethframe *frame, long data_len);

//struct firefly_connection *firefly_transport_connection_eth_stellaris_new(
		//struct firefly_transport_llp *llp, char *mac_address);

int firefly_transport_connection_eth_stellaris_free_event(void *event_arg);

void firefly_transport_connection_eth_stellaris_free(
		struct firefly_connection *conn);

void firefly_transport_eth_stellaris_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn);

#endif
