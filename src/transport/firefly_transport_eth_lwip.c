/**
 * @file
 * @brief The public API of the transport Ethernet LWIP with specific structures
 * and functions.
 */

#include <transport/firefly_transport_eth_lwip.h>
#include "transport/firefly_transport_eth_lwip_private.h"

#include <utils/firefly_event_queue.h>
#include <utils/firefly_errors.h>
#include <utils/cppmacros.h>
#include <transport/firefly_transport.h>

#include "utils/firefly_event_queue_private.h"
#include "transport/firefly_transport_private.h"
#include "protocol/firefly_protocol_private.h"

#include "udp_printf.h"

#define ERROR_STR_MAX_LEN      (256)
#define PBUF_DEFAULT_SIZE      (10)

static struct firefly_transport_llp *llp = NULL;

// NOTE: Expects that the LWIP TCP/IP stack is set up when called.
struct firefly_transport_llp *firefly_transport_llp_eth_lwip_new(int iface_num,
		firefly_on_conn_recv_eth_lwip on_conn_recv)
{
	UNUSED_VAR(iface_num);
	struct transport_llp_eth_lwip *llp_eth_lwip;

	llp_eth_lwip->recv_buf = pbuf_alloc(PBUF_LINK,
					    PBUF_DEFAULT_SIZE,
					    PBUF_RAM);

	llp_eth_lwip = malloc(sizeof(*llp_eth_lwip));

	llp_eth_lwip->on_conn_recv = on_conn_recv;
	llp_eth_lwip->recv_buf = NULL;
	llp_eth_lwip->recv_buf_size = 0;

	llp = malloc(sizeof(struct firefly_transport_llp));
	llp->llp_platspec = llp_eth_lwip;
	llp->conn_list = NULL;

	return llp;
}

// TODO: Implement correctly!
void firefly_recieve_ethernet(struct netif *netif, struct pbuf *pbuf)
{
	struct ethhdr *eth_header;
	UNUSED_VAR(netif);

	eth_header = (struct ethhdr*) pbuf->payload;

	for (size_t i = 0; i < pbuf->len - 2; ++i) {
		if (i != 0 && (i % 16) == 0) {
			udpPrintf("\n");
		} else if (i != 0 && (i % 8) == 0) {
			udpPrintf("  ");
		}
		unsigned int value = (unsigned int) ((unsigned char *) pbuf->payload)[i + 2];
		udpPrintf("%2x ", value);
	}
	udpPrintf("\nEOF\n");
}

void firefly_transport_llp_eth_lwip_free(struct firefly_transport_llp **llp)
{
	struct transport_llp_eth_lwip *llp_eth;
	llp_eth = (struct transport_llp_eth_lwip *) (*llp)->llp_platspec;
	// close(llp_eth->socket);

	struct llp_connection_list_node *head = (*llp)->conn_list;
	struct llp_connection_list_node *tmp = NULL;
	while (head != NULL) {
		tmp = head->next;
		firefly_transport_connection_eth_lwip_free_event(head->conn);
		free(head);
		head = tmp;
	}

	pbuf_free(llp_eth->recv_buf);
	free(llp_eth);
	free(*llp);
	*llp = NULL;
}

/*struct firefly_connection *firefly_transport_connection_eth_lwip_new(*/
		/*struct firefly_transport_llp *llp, char *mac_address)*/
/*{*/
	/*return NULL;*/
/*}*/

void firefly_transport_connection_eth_lwip_free(struct firefly_connection *conn)
{
	struct firefly_event *ev = firefly_event_new(1,
			firefly_transport_connection_eth_lwip_free_event,
			conn);
	conn->event_queue->offer_event_cb(conn->event_queue, ev);
}

int firefly_transport_connection_eth_lwip_free_event(void *event_arg)
{
	struct firefly_connection *conn;
	struct protocol_connection_eth_lwip *conn_eth;

	conn = (struct firefly_connection *) event_arg;
	conn_eth =
		(struct protocol_connection_eth_lwip *)
		conn->transport_conn_platspec;

	free(conn_eth->remote_addr);
	free(conn_eth);
	firefly_connection_free(&conn);

	return 0;
}

struct firefly_connection *firefly_transport_connection_eth_lwip_open(
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
	// err = ioctl(((struct transport_llp_eth_lwip *)llp->llp_platspec)->socket,
	// 		SIOCGIFINDEX, &ifr);
	// if(err < 0){
	// 		firefly_error(FIREFLY_ERROR_SOCKET, 3,
	// 				"Failed in %s() on line %d.\n", __FUNCTION__,
	// 				__LINE__);
	// 		return NULL;
	// }

	// /* Alloc connection structs */
	// struct protocol_connection_eth_lwip *conn_eth;
	// conn_eth = malloc(sizeof(struct protocol_connection_eth_lwip));
	// struct firefly_connection *conn = firefly_connection_new_register(
	// 		on_channel_opened, on_channel_closed, on_channel_recv,
	// 		firefly_transport_eth_lwip_write, event_queue,
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
	// 	((struct transport_llp_eth_lwip *)llp->llp_platspec)->socket;

	// add_connection_to_llp(conn, llp);

	// return conn;
	return NULL;
}

void firefly_transport_connection_eth_lwip_close(
		struct firefly_connection *conn)
{
	struct protocol_connection_eth_lwip *conn_eth =
		(struct protocol_connection_eth_lwip *)
			conn->transport_conn_platspec;
	conn_eth->open = FIREFLY_CONNECTION_CLOSED;
}

void firefly_transport_eth_lwip_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn)
{
	struct protocol_connection_eth_lwip *conn_eth =
		(struct protocol_connection_eth_lwip *) conn->transport_conn_platspec;
	UNUSED_VAR(conn_eth);
	UNUSED_VAR(data);
	UNUSED_VAR(data_size);

	// TODO: Send data here.
}

// TODO: Do we need this?
//void recv_buf_resize(struct transport_llp_eth_lwip *llp_eth, size_t new_size)
//{
//	if (new_size > llp_eth->recv_buf_size) {
//		llp_eth->recv_buf = realloc(llp_eth->recv_buf, new_size);
//		if (llp_eth->recv_buf == NULL) {
//			llp_eth->recv_buf_size = 0;
//			firefly_error(FIREFLY_ERROR_ALLOC, 3,
//					"Failed in %s() on line %d.\n", __FUNCTION__,
//					__LINE__);
//		} else {
//			llp_eth->recv_buf_size = new_size;
//		}
//	}
//}

void firefly_transport_eth_lwip_read(struct firefly_transport_llp *llp)
{
	UNUSED_VAR(llp);
}

