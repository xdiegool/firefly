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

// Include scheduling functions to manage reading from ethernet
#include <FreeRTOS.h>
#include <task.h>
// Needed for ethernet.h since it doesn't include the header files it needs...
#include <driverlib/inc/hw_types.h>
#include <driverlib/ethernet.h>
#include <driverlib/inc/hw_memmap.h>

void sprint_mac(char *buf, unsigned char *addr);

unsigned char *build_ethernet_frame(unsigned char *src, unsigned char *dest,
		unsigned char *data, size_t data_len)
{
	if (data_len > STELLARIS_ETH_MAX_DATA_LEN) {
		 firefly_error(FIREFLY_ERROR_ALLOC, 3,
				 "Too much data for ethernet frame! In %s() on"
				 "line %d.\n", __func__, __LINE__);
		 return NULL;
	}

	unsigned char *frame = calloc(1, data_len + ETH_HEADER_LEN);
	if (frame == NULL) {
		 firefly_error(FIREFLY_ERROR_ALLOC, 3,
				 "Failed while allocating eth_frame in %s() on"
				 "line %d.\n", __func__, __LINE__);
		 return NULL;
	}

	memcpy(frame, dest, ETH_ADDR_LEN);
	memcpy(frame + ETH_ADDR_LEN, src, ETH_ADDR_LEN);
	frame[ETH_PROTO_OFFSET] = (unsigned char) (FIREFLY_ETH_PROTOCOL >> 8);
	frame[ETH_PROTO_OFFSET + 1] = (unsigned char) (FIREFLY_ETH_PROTOCOL & 0x00FF);
	memcpy(frame + ETH_DATA_OFFSET, data, data_len);

	return frame;
}

int send_ethernet_frame(unsigned char *frame, long frame_len)
{
	long res = EthernetPacketPut(ETH_BASE, frame, frame_len);
	free(frame);

	if (res < 0) {
		firefly_error(FIREFLY_ERROR_SOCKET, 1,
				"Failed while sending!\n");
		return -1;
	}

	return 0;
}

// NOTE: Expects the Stellaris Ethernet controller to be fully up before called.
struct firefly_transport_llp *firefly_transport_llp_eth_stellaris_new(
		unsigned char *src_addr,
		firefly_on_conn_recv_eth_stellaris on_conn_recv)
{
	struct firefly_transport_llp *llp;
	struct transport_llp_eth_stellaris *llp_eth_stellaris;

	llp_eth_stellaris               = malloc(sizeof(*llp_eth_stellaris));
	if (llp_eth_stellaris == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
				"Failed in %s() on line %d.\n", __FUNCTION__,
				__LINE__);
		return NULL;
	}
	llp_eth_stellaris->on_conn_recv = on_conn_recv;
	llp_eth_stellaris->recv_buf     = malloc(STELLARIS_ETH_MAX_FRAME_LEN);
	memcpy(llp_eth_stellaris->src_addr, src_addr, ETH_ADDR_LEN);

	if (llp_eth_stellaris->recv_buf == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
				"Failed in %s() on line %d.\n", __FUNCTION__,
				__LINE__);
		free(llp_eth_stellaris);
		return NULL;
	}

	llp               = malloc(sizeof(struct firefly_transport_llp));
	if (llp == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
				"Failed in %s() on line %d.\n", __FUNCTION__,
				__LINE__);
		free(llp_eth_stellaris);
		free(llp_eth_stellaris->recv_buf);
		return NULL;
	}
	llp->llp_platspec = llp_eth_stellaris;
	llp->conn_list    = NULL;

	return llp;
}

void firefly_transport_llp_eth_stellaris_free(struct firefly_transport_llp **llp)
{
	struct transport_llp_eth_stellaris *llp_eth;
	struct llp_connection_list_node *head;
	struct llp_connection_list_node *tmp;

	llp_eth = (struct transport_llp_eth_stellaris *) (*llp)->llp_platspec;

	head = (*llp)->conn_list;
	tmp = NULL;
	while (head != NULL) {
		tmp = head->next;
		firefly_transport_connection_eth_stellaris_free_event(head->conn);
		free(head);
		head = tmp;
	}

	free(llp_eth);
	free(*llp);
	*llp = NULL;
}

void firefly_transport_connection_eth_stellaris_free(
		struct firefly_connection *conn)
{
	struct firefly_event *ev = firefly_event_new(FIREFLY_PRIORITY_LOW,
			firefly_transport_connection_eth_stellaris_free_event,
			conn);
	conn->event_queue->offer_event_cb(conn->event_queue, ev);
}

void firefly_transport_connection_eth_stellaris_free_cb(
		struct firefly_connection *conn)
{
	struct protocol_connection_eth_stellaris *conn_eth;
	conn_eth =
		(struct protocol_connection_eth_stellaris *)
			conn->transport_conn_platspec;
	// Remove conn from llp
	free(conn_eth);
}

int firefly_transport_connection_eth_stellaris_free_event(void *event_arg)
{
	struct firefly_connection *conn;
	conn = (struct firefly_connection *) event_arg;

	firefly_transport_connection_eth_stellaris_free_cb(conn);
	firefly_connection_free(&conn);
	return 0;
}

struct firefly_connection *firefly_transport_connection_eth_stellaris_open(
				firefly_channel_is_open_f on_channel_opened,
				firefly_channel_closed_f on_channel_closed,
				firefly_channel_accept_f on_channel_recv,
				struct firefly_event_queue *event_queue,
				unsigned char *mac_address,
				struct firefly_transport_llp *llp)
{
	struct transport_llp_eth_stellaris *llp_eth =
		(struct transport_llp_eth_stellaris *) llp->llp_platspec;

	struct protocol_connection_eth_stellaris *conn_eth =
		malloc(sizeof(struct protocol_connection_eth_stellaris));

	struct firefly_connection *conn = firefly_connection_new_register(
			on_channel_opened, on_channel_closed, on_channel_recv,
			firefly_transport_eth_stellaris_write, event_queue,
			conn_eth, true);
	if (conn == NULL || conn_eth == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
				"Failed in %s() on line %d.\n", __FUNCTION__,
				__LINE__);
		free(conn_eth);
		free(conn);

		return NULL;
	}

	memcpy(conn_eth->remote_addr, mac_address, ETH_ADDR_LEN);
	memcpy(conn_eth->src_addr, llp_eth->src_addr, ETH_ADDR_LEN);

	add_connection_to_llp(conn, llp);

	return conn;
}

void firefly_transport_eth_stellaris_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn)
{
	struct protocol_connection_eth_stellaris *conn_eth;
	int res;
	unsigned char *frame;

	conn_eth = (struct protocol_connection_eth_stellaris *)
		conn->transport_conn_platspec;

	frame = build_ethernet_frame(conn_eth->src_addr, conn_eth->remote_addr, data,
			data_size);
	if (frame == NULL) {
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
			"Error while buidling Ethernet frame! In %s() on"
			"line %d.\n", __func__, __LINE__);
	} else {
		res = send_ethernet_frame(frame, data_size + ETH_HEADER_LEN);
		if (res != 0) {
			firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Error while sending data! In %s() online %d.\n",
				__func__, __LINE__);
		}
	}
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
	struct protocol_connection_eth_stellaris *conn_eth;
	unsigned char *addr1;
	unsigned char *addr2;

	conn_eth = (struct protocol_connection_eth_stellaris *)
		conn->transport_conn_platspec;

	addr1 = conn_eth->remote_addr;
	addr2 = (unsigned char *) context;

	return !memcmp(addr1, addr2, ETH_ADDR_LEN);
}

void firefly_transport_eth_stellaris_read(struct firefly_transport_llp *llp)
{
	struct transport_llp_eth_stellaris *llp_eth;
	long len;
	struct firefly_connection *conn;

	llp_eth = (struct transport_llp_eth_stellaris *) llp->llp_platspec;

	while (!EthernetPacketAvail(ETH_BASE)) {
		vTaskDelay(0);
	}
	len = EthernetPacketGet(ETH_BASE, llp_eth->recv_buf,
			STELLARIS_ETH_MAX_FRAME_LEN);

	if (len < 0) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
			 "Too much data for buffer! In %s() on"
			 "line %d.\n", __func__, __LINE__);

		return;
	}

	// Ignore anything that's not our protocol.
	short proto = get_ethernet_proto(llp_eth->recv_buf);
	if (proto != FIREFLY_ETH_PROTOCOL) {
		firefly_error(FIREFLY_ERROR_USER_DEF, 1, "Not Firefly data.\n");
		return;
	}

	// Find existing connection or create new.
	conn = find_connection(llp, llp_eth->recv_buf + ETH_SRC_OFFSET,
			connection_eq_remmac);
	if (conn == NULL)
	{
		if (llp_eth->on_conn_recv != NULL) {
			conn = llp_eth->on_conn_recv(llp,
					llp_eth->recv_buf + ETH_SRC_OFFSET);
		} else {
			char addr[18];

			sprint_mac(addr, llp_eth->recv_buf + ETH_SRC_OFFSET);
			firefly_error(FIREFLY_ERROR_MISSING_CALLBACK, 4,
					"Cannot accept supposed connection from"
					" mac: %s.\n (in %s() at %s:%d)", addr,
					__func__, __FILE__, __LINE__);
		}
	}

	// Existing or newly created conn. Passing data to procol layer.
	if (conn != NULL) {
		protocol_data_received(conn, llp_eth->recv_buf + ETH_DATA_OFFSET,
				len - ETH_HEADER_LEN);
	}

#if DEBUG
	{
		char buf[18];

		sprint_mac(buf, llp_eth->recv_buf + ETH_DST_OFFSET);
		firefly_error(FIREFLY_ERROR_USER_DEF, 2, "Recv'd %d B data of "
				"type 0x%x to %s\n", len,
				get_ethernet_proto(llp_eth->recv_buf),
				buf);
	}
#endif
}
