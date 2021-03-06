#include <transport/firefly_transport_udp_lwip.h>
#include "transport/firefly_transport_udp_lwip_private.h"

#include <string.h>
#ifdef ARM_CORTEXM3_CODESOURCERY
	#include <ustdlib.h> // Needed for usnprintf.
#endif

#include <transport/firefly_transport.h>
#include <utils/firefly_errors.h>
#include <utils/firefly_event_queue.h>
#include <lwip/ip_addr.h>
#include <lwip/udp.h>

#include "protocol/firefly_protocol_private.h"
#include "transport/firefly_transport_private.h"
#include "utils/firefly_event_queue_private.h"
#include "utils/cppmacros.h"

/**
 * @brief The callback called by LWIP when new data has been fetched.
 *
 * This function constructs a #firefly_event_llp_read_udp_lwip with all
 * the data of the read and creates an event that does the rest.
 *
 * @param recv_arg A void pointer used to carry data through LWIP, in
 * this case our LLP.
 * @param upcb UDP Program Control Block, unused in this function.
 * @param pbuf A data structure with the data that has been read.
 * @param remote_ip_addr The IP address from where the data came.
 * @param remote_port The port number on which the data entered.
 */
static void udp_lwip_recv_callback(void *recv_arg, struct udp_pcb *upcb,
		struct pbuf *pbuf, struct ip_addr *remote_ip_addr,
		u16_t remote_port)
{
	UNUSED_VAR(upcb);
	struct firefly_transport_llp *llp = recv_arg;
	struct transport_llp_udp_lwip *llp_udp = llp->llp_platspec;

	struct firefly_event_llp_read_udp_lwip *ev_a = malloc(sizeof(*ev_a));
	if (llp_udp == NULL) {
		FFL(FIREFLY_ERROR_ALLOC);
		return;
	}
	ev_a->llp	= llp;
	ev_a->ip_addr	= remote_ip_addr;
	ev_a->p		= pbuf;
	ev_a->port	= remote_port;

	llp_udp->event_queue->offer_event_cb(llp_udp->event_queue,
			FIREFLY_PRIORITY_HIGH,
			firefly_transport_udp_lwip_read_event, ev_a, 0, NULL);
}

int firefly_transport_udp_lwip_read_event(void *event_arg)
{
	struct firefly_event_llp_read_udp_lwip *ev_a = event_arg;
	struct transport_llp_udp_lwip *llp_udp = ev_a->llp->llp_platspec;

	// Find existing connection or create new.
	struct firefly_connection *conn = find_connection(ev_a->llp,
			ev_a->ip_addr, transport_udp_lwip_conn_eq_ipaddr);

	if (conn == NULL) {
		char *ip_str = ip_addr_to_str(ev_a->ip_addr);
		if (llp_udp->on_conn_recv != NULL &&
				llp_udp->on_conn_recv(ev_a->llp, ip_str, ev_a->port)) {
			free(ip_str);
			return llp_udp->event_queue->offer_event_cb(llp_udp->event_queue,
					FIREFLY_PRIORITY_HIGH,
					firefly_transport_udp_lwip_read_event, ev_a, 0, NULL);
		}
		free(ip_str);
	} else {
		unsigned char *data = malloc(ev_a->p->len);
		if (data) {
			memcpy(data, ev_a->p->payload, ev_a->p->len);
			ev_a->llp->protocol_data_received_cb(conn, data, ev_a->p->len);
		}
	}
	pbuf_free(ev_a->p);
	free(ev_a);
	return 0;
}

struct firefly_transport_llp *firefly_transport_llp_udp_lwip_new(
		char *local_ip_addr, unsigned short local_port,
		firefly_on_conn_recv_udp_lwip_f on_conn_recv,
		struct firefly_event_queue *event_queue)
{
	struct transport_llp_udp_lwip *llp_udp =
				malloc(sizeof(struct transport_llp_udp_lwip));
	if (llp_udp == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 2,
				"Failed in %s().\n", __FUNCTION__);
		return NULL;
	}

	llp_udp->upcb = udp_new();
	if (llp_udp->upcb == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 2,
				"Failed to allocate udp_pcb in"
				" %s().\n", __FUNCTION__);
		free(llp_udp);
		return NULL;
	}
	struct ip_addr *ip_lwip = str_to_ip_addr(local_ip_addr);
	if (ip_lwip == NULL) {
		free(llp_udp);
		return NULL;
	}
	err_t u_err = udp_bind(llp_udp->upcb, ip_lwip, local_port);
	if (u_err == ERR_USE) {
		firefly_error(FIREFLY_ERROR_LLP_BIND, 1,
				"UDP port %u already bound!\n", local_port);
		free(llp_udp);
		free(ip_lwip);
		return NULL;
	}

	llp_udp->local_ip_addr = ip_lwip;
	llp_udp->local_port = local_port;
	llp_udp->on_conn_recv = on_conn_recv;
	llp_udp->event_queue = event_queue;

	struct firefly_transport_llp *llp = malloc(sizeof(*llp));
	if (llp == NULL) {
		FFL(FIREFLY_ERROR_ALLOC);
		udp_remove(llp_udp->upcb);
		free(llp_udp);
		free(ip_lwip);
		return NULL;
	}

	llp->llp_platspec = llp_udp;
	llp->conn_list = NULL;
	// Set recieve callback.
	udp_recv(llp_udp->upcb, udp_lwip_recv_callback, llp);

	return llp;
}

void firefly_transport_llp_udp_lwip_free(struct firefly_transport_llp *llp)
{
	struct transport_llp_udp_lwip *llp_lwip = llp->llp_platspec;

	int ret = llp_lwip->event_queue->offer_event_cb(llp_lwip->event_queue,
			FIREFLY_PRIORITY_LOW,
			firefly_transport_llp_udp_lwip_free_event, llp, 0, NULL);
	FFLIF(ret < 0, FIREFLY_ERROR_ALLOC);
}

int firefly_transport_llp_udp_lwip_free_event(void *event_arg)
{
	struct firefly_transport_llp *llp = event_arg;
	struct transport_llp_udp_lwip *llp_udp = llp->llp_platspec;

	// Delete all connections.
	bool empty = true;
	struct llp_connection_list_node *head = llp->conn_list;
	while (head != NULL) {
		empty = false;
		firefly_connection_close(head->conn);
		head = head->next;
	}
	if (empty) {
		udp_remove(llp_udp->upcb);
		free(llp_udp->local_ip_addr);
		free(llp_udp);
		free(llp);
	} else {
		firefly_transport_llp_udp_lwip_free(llp);
	}
	return 0;
}

static int connection_open(struct firefly_connection *conn)
{
	struct firefly_transport_connection_udp_lwip *tcul;
	tcul = conn->transport->context;
	add_connection_to_llp(conn, tcul->llp);
	return 0;
}

static int connection_close(struct firefly_connection *conn)
{
	struct firefly_transport_connection_udp_lwip *tcul;
	tcul = conn->transport->context;

	remove_connection_from_llp(tcul->llp, conn,
			firefly_connection_eq_ptr);
	free(tcul->remote_ip_addr);
	free(tcul);
	free(conn->transport);
	return 0;
}

// TODO we should not have to memcpy the data to write. Can we make memory alloc
// transport specific or avoid this problem somehow?
void firefly_transport_udp_lwip_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id)
{
	struct firefly_transport_connection_udp_lwip *conn_udp;
	conn_udp = conn->transport->context;
	struct pbuf *pbuf = pbuf_alloc(PBUF_TRANSPORT, data_size, PBUF_RAM);
	if (pbuf == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
				"Failed to allocate a pbuf in  %s() on"
				"line %d.\n", __FUNCTION__, __LINE__);
	}
	memcpy(pbuf->payload, data, data_size);

	err_t err = udp_sendto(conn_udp->upcb, pbuf, conn_udp->remote_ip_addr,
			conn_udp->remote_port);
	if (err != ERR_OK) {
		FFL(FIREFLY_ERROR_TRANS_WRITE);
		firefly_connection_raise_later(conn,
				FIREFLY_ERROR_TRANS_WRITE, "udp_sendto() failed");
	}
	pbuf_free(pbuf);
	if (important && id != NULL) {
		// TODO
	}
}

void firefly_transport_udp_lwip_ack(unsigned char pkt_id,
		struct firefly_connection *conn)
{
	UNUSED_VAR(pkt_id);
	UNUSED_VAR(conn);
}


struct firefly_transport_connection *firefly_transport_connection_udp_lwip_new(
		struct firefly_transport_llp *llp,
		char *remote_ip_addr,
		unsigned short remote_port)
{
	struct firefly_transport_connection *tc;
	struct firefly_transport_connection_udp_lwip *tcul;
	struct transport_llp_udp_lwip *llp_lwip = llp->llp_platspec;

	tc = malloc(sizeof(*tc));
	tcul = malloc(sizeof(*tcul));
	if (tc == NULL || tcul == NULL) {
		free(tc);
		free(tcul);
		return NULL;
	}

	struct ip_addr *ip_lwip = str_to_ip_addr(remote_ip_addr);
	if (ip_lwip == NULL) {
		free(ip_lwip);
		free(tc);
		free(tcul);
		return NULL;
	}
	tcul->upcb = llp_lwip->upcb;
	tcul->remote_ip_addr = ip_lwip;
	tcul->remote_port = (u16_t) remote_port;
	tcul->llp = llp;
	tc->context = tcul;
	tc->open = connection_open;
	tc->close = connection_close;
	tc->write = firefly_transport_udp_lwip_write;
	tc->ack = firefly_transport_udp_lwip_ack;

	return tc;
}

bool transport_udp_lwip_conn_eq_ipaddr(struct firefly_connection *conn,
		void *context)
{
	return ip_addr_cmp((struct ip_addr *) context,
			((struct firefly_transport_connection_udp_lwip *)
			 conn->transport->context)->remote_ip_addr);
}

struct ip_addr *str_to_ip_addr(const char *ip_str)
{
	unsigned char ip_parts[4];

	size_t ip_str_len = strlen(ip_str);
	char ip_str_cpy[ip_str_len + 1];
	strncpy(ip_str_cpy, ip_str, ip_str_len);
	ip_str_cpy[ip_str_len] = '\0';
	char *pch;
	for (unsigned char i = 0; i < 4; ++i) {
		const char *delim = ".";
		if (i == 0) {
			pch = strtok(ip_str_cpy, delim);
		} else {
			pch = strtok(NULL, delim);
		}
		if (i != 3 && pch == NULL) {
			firefly_error(FIREFLY_ERROR_IP_PARSE, 3,
					"Failed to parse IP address \"%s\""
					"in %s().\n", ip_str, __FUNCTION__);
			return NULL;
		}
		char *endptr;
		ip_parts[i] = strtol(pch, &endptr, 10);
		if (endptr == pch || *endptr != '\0' || errno == ERANGE) {
			firefly_error(FIREFLY_ERROR_IP_PARSE, 3,
					"Failed to parse IP address \"%s\""
					"in %s().\n", ip_str, __FUNCTION__);
			return NULL;
		}
	}

	struct ip_addr *ip_addr = malloc(sizeof(*ip_addr));
	if (ip_addr == NULL) {
		FFL(FIREFLY_ERROR_ALLOC);
		return NULL;
	}
	IP4_ADDR(ip_addr,ip_parts[0],ip_parts[1],ip_parts[2],ip_parts[3]);
	return ip_addr;
}

char *ip_addr_to_str(struct ip_addr *ip_addr)
{
	size_t ipv4_size = 4*3 + 3 + 1;
	char *ip_str = malloc(ipv4_size); // Max length of IPv4 address string.
	if (ip_str == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 2,
				"Could no allocate ip_str in function %s.\n",
				__FUNCTION__);
		return NULL;
	}

	snprintf(ip_str, ipv4_size, "%d.%d.%d.%d",
			ip4_addr1(ip_addr),
			ip4_addr2(ip_addr),
			ip4_addr3(ip_addr),
			ip4_addr4(ip_addr));

	return ip_str;
}
