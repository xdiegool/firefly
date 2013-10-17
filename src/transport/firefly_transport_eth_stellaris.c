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


short get_ethernet_proto(unsigned char *frame) {
	short proto = 0;
	proto = frame[ETH_PROTO_OFFSET] << 8;
	proto |= frame[ETH_PROTO_OFFSET + 1];

	return proto;
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
	buf[strp - 1] = '\0'; // Replace last colon with terminator.
}

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
		struct firefly_event_queue *event_queue,
		firefly_on_conn_recv_eth_stellaris on_conn_recv)
{
	struct firefly_transport_llp *llp;
	struct transport_llp_eth_stellaris *llp_eth_stellaris;

	llp_eth_stellaris = malloc(sizeof(*llp_eth_stellaris));
	if (llp_eth_stellaris == NULL) {
		FFL(FIREFLY_ERROR_ALLOC);
		return NULL;
	}
	llp_eth_stellaris->on_conn_recv = on_conn_recv;
	llp_eth_stellaris->event_queue  = event_queue;
	memcpy(llp_eth_stellaris->src_addr, src_addr, ETH_ADDR_LEN);

	llp = malloc(sizeof(*llp));
	if (llp == NULL) {
		FFL(FIREFLY_ERROR_ALLOC);
		free(llp_eth_stellaris);
		return NULL;
	}
	llp->llp_platspec = llp_eth_stellaris;
	llp->conn_list    = NULL;

	return llp;
}

void firefly_transport_llp_eth_stellaris_free(struct firefly_transport_llp *llp)
{
	struct transport_llp_eth_stellaris *llp_eth;
	llp_eth = llp->llp_platspec;
	int ret = llp_eth->event_queue->offer_event_cb(llp_eth->event_queue,
			FIREFLY_PRIORITY_LOW,
			firefly_transport_llp_eth_stellaris_free_event, llp, 0, NULL);
	FFLIF(ret < 0, FIREFLY_ERROR_ALLOC)
}

int firefly_transport_llp_eth_stellaris_free_event(void *event_arg)
{
	struct firefly_transport_llp *llp = event_arg;

	bool empty = true;
	// Close all connections.
	struct llp_connection_list_node *head = llp->conn_list;
	while (head != NULL) {
		empty = false;
		firefly_connection_close(head->conn);
		head = head->next;
	}
	if (empty) {
		struct transport_llp_eth_stellaris *llp_eth = llp->llp_platspec;
		free(llp_eth);
		free(llp);
	} else {
		firefly_transport_llp_eth_stellaris_free(llp);
	}

	return 0;
}

static int connection_open(struct firefly_connection *conn)
{
	struct firefly_transport_connection_eth_stellaris *tces;
	tces = conn->transport->context;
	add_connection_to_llp(conn, tces->llp);
	return 0;
}

static int connection_close(struct firefly_connection *conn)
{
	struct firefly_transport_connection_eth_stellaris *tces;
	tces = conn->transport->context;

	remove_connection_from_llp(tces->llp, conn,
			firefly_connection_eq_ptr);
	free(tces);
	free(conn->transport);
	return 0;
}

struct firefly_transport_connection *
firefly_transport_connection_eth_stellaris_new(
		struct firefly_transport_llp *llp,
		unsigned char *mac_address)
{
	struct firefly_transport_connection *tc;
	struct firefly_transport_connection_eth_stellaris *conn_eth;
	struct transport_llp_eth_stellaris *llp_eth;

	llp_eth = llp->llp_platspec;
	tc = malloc(sizeof(*tc));
	conn_eth = malloc(sizeof(*conn_eth));
	if (tc == NULL || conn_eth == NULL) {
		FFL(FIREFLY_ERROR_ALLOC);
		free(conn_eth);
		free(tc);
		return NULL;
	}

	memcpy(conn_eth->remote_addr, mac_address, ETH_ADDR_LEN);
	conn_eth->llp = llp;
	tc->context = conn_eth;
	tc->open = connection_open;
	tc->close = connection_close;
	tc->write = firefly_transport_eth_stellaris_write;
	tc->ack = firefly_transport_eth_stellaris_ack;

	return tc;
}

void firefly_transport_eth_stellaris_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id)
{
	struct firefly_transport_connection_eth_stellaris *conn_eth;
	struct transport_llp_eth_stellaris *llp_eth;
	int res;
	unsigned char *frame;

	UNUSED_VAR(important);
	UNUSED_VAR(id);
	conn_eth = conn->transport->context;
	llp_eth = conn_eth->llp->llp_platspec;

	frame = build_ethernet_frame(llp_eth->src_addr, conn_eth->remote_addr,
			data, data_size);
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

void firefly_transport_eth_stellaris_ack(unsigned char pkg_id,
		struct firefly_connection *conn)
{
	UNUSED_VAR(pkg_id);
	UNUSED_VAR(conn);
}

bool connection_eq_remmac(struct firefly_connection *conn, void *context)
{
	struct firefly_transport_connection_eth_stellaris *conn_eth;
	unsigned char *addr1;
	unsigned char *addr2;

	conn_eth = conn->transport->context;

	addr1 = conn_eth->remote_addr;
	addr2 = context;

	return !memcmp(addr1, addr2, ETH_ADDR_LEN);
}

void firefly_transport_eth_stellaris_read(struct firefly_transport_llp *llp)
{
	struct transport_llp_eth_stellaris *llp_eth;
	unsigned char *eth_packet;
	long len;
	// Unfortunately there's no way to find the size of the next packet so
	// we have to allocate as much as possible...
	eth_packet = malloc(STELLARIS_ETH_MAX_FRAME_LEN);
	if (eth_packet == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
			 "Could not alloc buffer! In %s() on"
			 "line %d.\n", __func__, __LINE__);

		return;
	}

	while (!EthernetPacketAvail(ETH_BASE)) {
		// 0 means rescheduling of threads, needed to avoid deadlocks.
		vTaskDelay(0);
	}
	len = EthernetPacketGet(ETH_BASE, eth_packet,
			STELLARIS_ETH_MAX_FRAME_LEN);

	if (len < 0) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
			 "Too much data for buffer! In %s() on"
			 "line %d.\n", __func__, __LINE__);

		return;
	}

	// Ignore anything that's not our protocol.
	short proto = get_ethernet_proto(eth_packet);
	if (proto != FIREFLY_ETH_PROTOCOL) {
		free(eth_packet);
		return;
	}

	struct firefly_event_llp_read_eth_stellaris *ev_a = malloc(sizeof(*ev_a));
	if (ev_a == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
			 "Could not alloc event struct! In %s() on"
			 "line %d.\n", __func__, __LINE__);
		return;
	}
	ev_a->llp = llp;
	ev_a->len = len;
	ev_a->eth_packet = eth_packet;

	llp_eth = llp->llp_platspec;

	llp_eth->event_queue->offer_event_cb(llp_eth->event_queue,
			FIREFLY_PRIORITY_HIGH,
			firefly_transport_eth_stellaris_read_event, ev_a, 0, NULL);

}

int firefly_transport_eth_stellaris_read_event(void *event_args)
{
	struct firefly_event_llp_read_eth_stellaris *ev_a = event_args;
	struct firefly_transport_llp *llp = ev_a->llp;
	struct transport_llp_eth_stellaris *llp_eth = llp->llp_platspec;
	struct firefly_connection *conn;

	// Find existing connection or create new.
	conn = find_connection(llp, ev_a->eth_packet + ETH_SRC_OFFSET,
			connection_eq_remmac);
	if (conn == NULL) {
		int64_t id = 0;
		unsigned char *mac_addr = ev_a->eth_packet + ETH_SRC_OFFSET;
		if (llp_eth->on_conn_recv != NULL &&
				(id = llp_eth->on_conn_recv(llp, mac_addr)) > 0) {
			return llp_eth->event_queue->offer_event_cb(llp_eth->event_queue,
					FIREFLY_PRIORITY_HIGH,
					firefly_transport_eth_stellaris_read_event, ev_a, 1, &id);
		} else {
			free(ev_a->eth_packet);
		}
	} else {
		protocol_data_received(conn,
				ev_a->eth_packet + ETH_DATA_OFFSET,
				ev_a->len - ETH_HEADER_LEN);
	}
	free(ev_a);

	return 0;
}

struct firefly_event_queue *firefly_transport_eth_stellaris_event_queue(
		struct firefly_transport_llp *llp)
{
	struct transport_llp_eth_stellaris *llp_eth = llp->llp_platspec;
	return llp_eth->event_queue;
}
