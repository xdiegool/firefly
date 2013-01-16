/**
 * @file
 * @brief The public API of the transport Ethernet Stellaris with specific structures
 * and functions.
 */

#include <transport/firefly_transport_eth_stellaris.h>
#include "transport/firefly_transport_eth_stellaris_private.h"

#include <utils/firefly_event_queue.h>
#include <utils/firefly_errors.h>
#include <utils/cppmacros.h>
#include <transport/firefly_transport.h>

#include "utils/firefly_event_queue_private.h"
#include "transport/firefly_transport_private.h"
#include "protocol/firefly_protocol_private.h"

#include <driverlib/ethernet.h>
#include <driverlib/inc/hw_memmap.h>

union ethframe *build_ethernet_frame(unsigned char *src,
		unsigned char *dest, unsigned char *data,
		size_t data_len)
{
	if (data_len > STELLARIS_ETH_MAX_DATA_LEN) {
		 firefly_error(FIREFLY_ERROR_ALLOC, 3,
				 "Too much data for ethernet frame! In %s() on"
				 "line %d.\n", __func__, __LINE__);
		 return NULL;
	}

	union ethframe *frame = calloc(1, sizeof(union ethframe));
	if (frame == NULL) {
		 firefly_error(FIREFLY_ERROR_ALLOC, 3,
				 "Failed while allocating eth_frame in %s() on"
				 "line %d.\n", __func__, __LINE__);
		 return NULL;
	}

	memcpy(&(frame->field.header.dest), &dest, ETH_ADDR_LEN);
	memcpy(&(frame->field.header.src), &src, ETH_ADDR_LEN);
	frame->field.header.proto = htons(FIREFLY_ETH_PROTOCOL);
	memcpy(frame->field.data, data, data_len);

	return frame;
}

int send_ethernet_frame(union ethframe *frame, long data_len) {
	long res = EthernetPacketPut(ETH_BASE, frame->buffer,
			data_len + ETH_HEADER_LEN);
	if (res == data_len + ETH_HEADER_LEN) {
		firefly_error(FIREFLY_ERROR_SOCKET, 1, "SUCCESS!!!!!!!!\n");
	} else {
		firefly_error(FIREFLY_ERROR_SOCKET, 1, "FAAAAAIL-WHALE!!!!!!!!\n");
	}

	return 0;
}

static struct firefly_transport_llp *llp;

// TODO remove this when done testing.
static union ethframe *f;
static size_t data_len;

void sendstuff() {
	send_ethernet_frame(f, data_len);
}

// NOTE: Expects the Stellaris Ethernet controller to be fully up before called.
struct firefly_transport_llp *firefly_transport_llp_eth_stellaris_new(
		firefly_on_conn_recv_eth_stellaris on_conn_recv)
{
	struct transport_llp_eth_stellaris *llp_eth_stellaris =
		malloc(sizeof(*llp_eth_stellaris));

	// TODO: Put this back if we need it.
	llp_eth_stellaris->on_conn_recv  = on_conn_recv;

	llp = malloc(sizeof(struct firefly_transport_llp));
	llp->llp_platspec = llp_eth_stellaris;
	llp->conn_list    = NULL;

	// TODO: Test data, remove later.
	unsigned char data[]             = "test";
	data_len                         = sizeof(data);
	unsigned char src[ETH_ADDR_LEN]  = {0x00,0x42,0x42,0x42,0x42,0x10};
	unsigned char dest[ETH_ADDR_LEN] = {0x00,0x26,0x2d,0xf9,0x5b,0xb8};
	f = build_ethernet_frame(src, dest, data, data_len);

	return llp;
}

// TODO: Implement correctly!
/*void firefly_recieve_ethernet(struct netif *netif, struct pbuf *pbuf)*/
/*{*/
	/*[>struct ethhdr *eth_header;<]*/
	/*UNUSED_VAR(netif);*/
	/*UNUSED_VAR(pbuf);*/

	/*[>eth_header = (struct ethhdr*) pbuf->payload;<]*/

	/*[>for (size_t i = 0; i < pbuf->len - 2; ++i) {<]*/
		/*[>if (i != 0 && (i % 16) == 0) {<]*/
			/*[>[>udpPrintf("\n");<]<]*/
		/*[>} else if (i != 0 && (i % 8) == 0) {<]*/
			/*[>[>udpPrintf("  ");<]<]*/
		/*[>}<]*/
		/*[>unsigned int value = (unsigned int) ((unsigned char *) pbuf->payload)[i + 2];<]*/
		/*[>[>udpPrintf("%2x ", value);<]<]*/
	/*[>}<]*/
	/*[>udpPrintf("\nEOF\n");<]*/
/*}*/

void firefly_transport_llp_eth_stellaris_free(struct firefly_transport_llp **llp)
{
	struct transport_llp_eth_stellaris *llp_eth;
	llp_eth = (struct transport_llp_eth_stellaris *) (*llp)->llp_platspec;
	// close(llp_eth->socket);

	struct llp_connection_list_node *head = (*llp)->conn_list;
	struct llp_connection_list_node *tmp = NULL;
	while (head != NULL) {
		tmp = head->next;
		firefly_transport_connection_eth_stellaris_free_event(head->conn);
		free(head);
		head = tmp;
	}

	/*free(llp_eth->recv_buf);*/
	free(llp_eth);
	free(*llp);
	*llp = NULL;
}

/*struct firefly_connection *firefly_transport_connection_eth_stellaris_new(*/
		/*struct firefly_transport_llp *llp, char *mac_address)*/
/*{*/
	/*return NULL;*/
/*}*/

void firefly_transport_connection_eth_stellaris_free(struct firefly_connection *conn)
{
	struct firefly_event *ev = firefly_event_new(1,
			firefly_transport_connection_eth_stellaris_free_event,
			conn);
	conn->event_queue->offer_event_cb(conn->event_queue, ev);
}

int firefly_transport_connection_eth_stellaris_free_event(void *event_arg)
{
	struct firefly_connection *conn;
	struct protocol_connection_eth_stellaris *conn_eth;

	conn = (struct firefly_connection *) event_arg;
	conn_eth =
		(struct protocol_connection_eth_stellaris *)
		conn->transport_conn_platspec;

	/*free(conn_eth->remote_addr);*/
	free(conn_eth);
	firefly_connection_free(&conn);

	return 0;
}

struct firefly_connection *firefly_transport_connection_eth_stellaris_open(
				firefly_channel_is_open_f on_channel_opened,
				firefly_channel_closed_f on_channel_closed,
				firefly_channel_accept_f on_channel_recv,
				struct firefly_event_queue *event_queue,
				char *mac_address,
				struct firefly_transport_llp *llp)
{
	UNUSED_VAR(on_channel_opened);
	UNUSED_VAR(on_channel_closed);
	UNUSED_VAR(on_channel_recv);
	UNUSED_VAR(event_queue);
	UNUSED_VAR(mac_address);
	UNUSED_VAR(llp);
	/* Get ifindex first to avoid mem alloc if it fails. */
	// int err;
	// struct ifreq ifr;
	// strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	// err = ioctl(((struct transport_llp_eth_stellaris *)llp->llp_platspec)->socket,
	// 		SIOCGIFINDEX, &ifr);
	// if(err < 0){
	// 		firefly_error(FIREFLY_ERROR_SOCKET, 3,
	// 				"Failed in %s() on line %d.\n", __FUNCTION__,
	// 				__LINE__);
	// 		return NULL;
	// }

	// /* Alloc connection structs */
	// struct protocol_connection_eth_stellaris *conn_eth;
	// conn_eth = malloc(sizeof(struct protocol_connection_eth_stellaris));
	// struct firefly_connection *conn = firefly_connection_new_register(
	// 		on_channel_opened, on_channel_closed, on_channel_recv,
	// 		firefly_transport_eth_stellaris_write, event_queue,
	// 		conn_eth, true);
	// if (conn == NULL || conn_eth == NULL) {
	// 	firefly_error(FIREFLY_ERROR_ALLOC, 3,
	// 			"Failed in %s() on line %d.\n", __FUNCTION__,
	// 			__LINE__);
	// 	free(conn_eth);
	// 	free(conn);
	// 	return NULL;
	// }
	// conn_eth->remote_addr = malloc(sizeof(struct sockaddr_ll));
	// if (conn_eth == NULL) {
	// 	firefly_error(FIREFLY_ERROR_ALLOC, 3,
	// 			"Failed in %s() on line %d.\n", __FUNCTION__,
	// 			__LINE__);
	// 	free(conn_eth);
	// 	return NULL;
	// }

	// memset(conn_eth->remote_addr, 0, sizeof(struct sockaddr_ll));
	// conn_eth->remote_addr->sll_family   = AF_PACKET;
	// conn_eth->remote_addr->sll_protocol = htons(FIREFLY_ETH_PROTOCOL);
	// /* Convert address to to binary data */
	// ether_aton_r(mac_address, (void*)&conn_eth->remote_addr->sll_addr);
	// conn_eth->remote_addr->sll_halen    = 6;
	// conn_eth->remote_addr->sll_ifindex  = ifr.ifr_ifindex;

	// conn_eth->socket =
	// 	((struct transport_llp_eth_stellaris *)llp->llp_platspec)->socket;

	// add_connection_to_llp(conn, llp);

	// return conn;
	return NULL;
}

void firefly_transport_connection_eth_stellaris_close(
		struct firefly_connection *conn)
{
	struct protocol_connection_eth_stellaris *conn_eth =
		(struct protocol_connection_eth_stellaris *)
			conn->transport_conn_platspec;
	conn_eth->open = FIREFLY_CONNECTION_CLOSED;
}

void firefly_transport_eth_stellaris_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn)
{
	struct protocol_connection_eth_stellaris *conn_eth =
		(struct protocol_connection_eth_stellaris *) conn->transport_conn_platspec;
	UNUSED_VAR(conn_eth);
	UNUSED_VAR(data);
	UNUSED_VAR(data_size);

	// TODO: Send data here.
}

void sprint_mac(char *buf, unsigned char *addr)
{
	int strp = 0;

	for (int i = 0; i < ETH_ADDR_LEN; i++) {
		char hex[] = {'0','1','2','3','4','5','6','7',
					  '8','9','A','B','C','D','E','F'};

		buf[strp++] = hex[(addr[i] & 0xF0) >> 4];
		buf[strp++] = hex[(addr[i] & 0x0F)];
		buf[strp++] = ':';
	}
	buf[strp - 1] = '\0';		/* Replace last colon with terminator. */
}

bool connection_eq_remmac(struct firefly_connection *conn, void *context)
{
	return !memcmp(((struct protocol_connection_eth_stellaris *) conn->transport_conn_platspec)->remote_addr,
				   (unsigned char *) context, ETH_ADDR_LEN);
}

void firefly_transport_eth_stellaris_read(struct firefly_transport_llp *llp)
{
	struct transport_llp_eth_stellaris *llp_eth =
		(struct transport_llp_eth_stellaris *) llp->llp_platspec;

	long len = EthernetPacketGet(ETH_BASE, llp_eth->recv_buf.buffer,
			STELLARIS_ETH_MAX_FRAME_LEN);

	if (len < 0) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
			 "Too much data for buffer! In %s() on"
			 "line %d.\n", __func__, __LINE__);

		return;
	}

	// Find existing connection or create new.
	// Ignore connections marked as closed.
	struct firefly_connection *conn = find_connection(llp, llp_eth->recv_buf.field.header.src, connection_eq_remmac);
	if (conn == NULL || !((struct protocol_connection_eth_stellaris *)
				conn->transport_conn_platspec)->open)
	{
		if (llp_eth->on_conn_recv != NULL) {
			conn = llp_eth->on_conn_recv(llp, llp_eth->recv_buf.field.header.src);
		} else {
			char addr[18];

			sprint_mac(addr, llp_eth->recv_buf.field.header.src);
			firefly_error(FIREFLY_ERROR_MISSING_CALLBACK, 4,
					      "Cannot accept supposed connection form mac: %s.\n (in %s() at %s:%d)",
						  addr, __FUNCTION__, __FILE__, __LINE__);
		}
	}

	// Existing or newly created conn. Passing data to procol layer.
	if (conn != NULL)
		protocol_data_received(conn, llp_eth->recv_buf.field.data, len - ETH_HEADER_LEN);

#if DEBUG
	{
		char buf[18];

		sprint_mac(buf, llp_eth->recv_buf.field.header.src);
		firefly_error(FIREFLY_ERROR_USER_DEF, 2, "Recv'd %d B data of type 0x%x from %s\n",
					  len,
					  ntohs(llp_eth->recv_buf.field.header.proto),
					  buf);
	}
#endif
}
