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

struct ethhdr {
	struct transport_eth_addr dest;
	struct transport_eth_addr src;
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
	unsigned long ethernet_base; // TODO: Should this be in connection?
	struct transport_eth_addr src_addr;
	unsigned char *recv_buf;
	size_t recv_buf_size;
	firefly_on_conn_recv_eth_stellaris on_conn_recv;
};

struct protocol_connection_eth_stellaris {
	struct transport_eth_addr remote_addr;
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
union ethframe *build_ethernet_frame(struct transport_eth_addr src,
		struct transport_eth_addr dest, unsigned char *data,
		size_t data_len);

/**
 * Helper function to send an Ethernet frame on the Ethernet interface
 * specified by llp->ethernet_base.
 *
 * @param llp The llp to send on,
 * @param frame The Ethernet frame to send.
 *
 * @return An \c int representing the success.
 * @retval 0 on success, -1 on failure.
 */
int send_ethernet_frame(struct firefly_transport_llp* llp,
		union ethframe *frame, long data_len);

//struct firefly_connection *firefly_transport_connection_eth_stellaris_new(
		//struct firefly_transport_llp *llp, char *mac_address);

int firefly_transport_connection_eth_stellaris_free_event(void *event_arg);

void firefly_transport_connection_eth_stellaris_free(struct firefly_connection *conn);

void firefly_transport_eth_stellaris_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn);

// void recv_buf_resize(struct transport_llp_eth_stellaris *llp_eth, size_t new_size);

// void get_mac_addr(struct sockaddr_ll *addr, char *mac_addr);

// bool connection_eq_addr(struct firefly_connection *conn, void *context);

#endif
