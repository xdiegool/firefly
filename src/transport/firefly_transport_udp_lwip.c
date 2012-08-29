#include <transport/firefly_transport_udp_lwip.h>
#include "firefly_transport_udp_lwip_private.h"

#include <string.h>
#ifdef ARM_CORTEXM3_CODESOURCERY
	#include <ustdlib.h> // Needed for usnprintf.
#endif

#include <transport/firefly_transport.h>
#include <utils/firefly_errors.h>
#include <lwip/ip_addr.h>
#include <lwip/udp.h>

#include "protocol/firefly_protocol_private.h"
#include "transport/firefly_transport_private.h"
#include "utils/cppmacros.h"

static void udp_lwip_recv_callback(void *recv_arg, struct udp_pcb *upcb,
		struct pbuf *pbuf, struct ip_addr *remote_ip_addr,
		u16_t remote_port)
{
	UNUSED_VAR(upcb);
	struct firefly_transport_llp *llp =
		(struct firefly_transport_llp *) recv_arg;
	struct transport_llp_udp_lwip *llp_udp =
		(struct transport_llp_udp_lwip *) llp->llp_platspec;

	// Find existing connection or create new.
	struct firefly_connection *conn = find_connection_by_addr(
			remote_ip_addr, llp);

	if (llp_udp->on_conn_recv == NULL) {
		// TODO feature consideration,
		// on_conn_recv == NULL => silently reject incomming connections
		firefly_error(FIREFLY_ERROR_MISSING_CALLBACK, 3,
			  "Failed in %s() on line %d. 'on_conn_recv missing'\n",
			  __FUNCTION__, __LINE__);
	}
	if (conn == NULL && llp_udp->on_conn_recv != NULL) {
		char *ip_str = ip_addr_to_str(remote_ip_addr);
		conn = llp_udp->on_conn_recv(llp, ip_str, remote_port);
		free(ip_str);
	}
	if (conn != NULL) { // Existing, not rejected and created conn.
		protocol_data_received(conn, pbuf->payload, pbuf->len);
	}
}

struct firefly_transport_llp *firefly_transport_llp_udp_lwip_new(
		char *local_ip_addr, unsigned short local_port,
		firefly_on_conn_recv_udp_lwip on_conn_recv)
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

	struct firefly_transport_llp *llp = malloc(sizeof(
						struct firefly_transport_llp));
	if (llp == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 2,
					  "Failed in %s().\n", __FUNCTION__);
	}

	llp->llp_platspec = llp_udp;
	llp->conn_list = NULL;
	// Set recieve callback.
	udp_recv(llp_udp->upcb, udp_lwip_recv_callback, llp);

	return llp;
}

void firefly_transport_llp_udp_lwip_free(struct firefly_transport_llp **llp)
{
	struct transport_llp_udp_lwip *llp_udp =
		(struct transport_llp_udp_lwip *) (*llp)->llp_platspec;

	// Delete all connections.
	struct llp_connection_list_node *head = (*llp)->conn_list;
	struct llp_connection_list_node *tmp = NULL;
	while (head != NULL) {
		tmp = head->next;
		firefly_transport_connection_udp_lwip_free(&head->conn);
		free(head);
		head = tmp;
	}
	udp_remove(llp_udp->upcb);
	free(llp_udp->local_ip_addr);
	free(llp_udp);
	free((*llp));
	*llp = NULL;
}

void firefly_transport_connection_udp_lwip_free(
		struct firefly_connection **conn)
{
	struct protocol_connection_udp_lwip *conn_udp =
		(struct protocol_connection_udp_lwip *)
		(*conn)->transport_conn_platspec;
	free(conn_udp->remote_ip_addr);
	free(conn_udp);
	firefly_connection_free(conn);
	*conn = NULL;
}

struct firefly_connection *firefly_transport_connection_udp_lwip_open(
				firefly_channel_is_open_f on_channel_opened,
				firefly_channel_closed_f on_channel_closed,
				firefly_channel_accept_f on_channel_recv,
				struct firefly_event_queue *event_queue,
				char *remote_ip_addr,
				unsigned short remote_port,
				struct firefly_transport_llp *llp)
{
	struct protocol_connection_udp_lwip *conn_udp =
		malloc(sizeof(struct protocol_connection_udp_lwip));

	struct firefly_connection *conn = firefly_connection_new(
			on_channel_opened, on_channel_closed,
			on_channel_recv, firefly_transport_udp_lwip_write,
			event_queue, conn_udp);
	if (conn == NULL || conn_udp == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
				"Failed in %s() on line %d.\n", __FUNCTION__,
				__LINE__);
		free(conn);
		free(conn_udp);
		return NULL;
	}

	struct ip_addr *ip_lwip = str_to_ip_addr(remote_ip_addr);
	if (ip_lwip == NULL) {
		return NULL;
	}
	conn_udp->remote_ip_addr = ip_lwip;
	conn_udp->remote_port = (u16_t) remote_port;
	conn_udp->open = FIREFLY_CONNECTION_OPEN;

	add_connection_to_llp(conn, llp);
	return conn;
}

void firefly_transport_connection_udp_lwip_close(
		struct firefly_connection *conn)
{
	struct protocol_connection_udp_lwip *conn_udp =
		(struct protocol_connection_udp_lwip *)
		conn->transport_conn_platspec;
	conn_udp->open = FIREFLY_CONNECTION_CLOSED;
}

int firefly_transport_udp_lwip_clean_up(struct firefly_transport_llp *llp)
{
	int nbr_closed = 0;
	struct llp_connection_list_node **head = &llp->conn_list;
	struct protocol_connection_udp_lwip *conn_udp;

	while (*head != NULL) {
		conn_udp = (struct protocol_connection_udp_lwip *)
			(*head)->conn->transport_conn_platspec;
		if (!conn_udp->open) {
			firefly_transport_connection_udp_lwip_free(
					&(*head)->conn);
			struct llp_connection_list_node *tmp = *head;
			*head = (*head)->next;
			free(tmp);
			nbr_closed++;
		} else {
			head = &(*head)->next;
		}
	}
	return nbr_closed;
}

// TODO we should not have to memcpy the data to write. Can we make memory alloc
// transport specific or avoid this problem somehow?
void firefly_transport_udp_lwip_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn)
{
	struct protocol_connection_udp_lwip *conn_udp =
		(struct protocol_connection_udp_lwip *)
		conn->transport_conn_platspec;
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
		firefly_error(FIREFLY_ERROR_TRANS_WRITE, 2,
				"Failed in %s().\n", __FUNCTION__);
	}
}

struct firefly_connection *find_connection_by_addr(struct ip_addr *ip_addr,
		struct firefly_transport_llp *llp)
{
	struct llp_connection_list_node *head = llp->conn_list;
	struct protocol_connection_udp_lwip *conn_udp;

	while (head != NULL) {
		conn_udp = (struct protocol_connection_udp_lwip *)
			head->conn->transport_conn_platspec;
		if (ip_addr_cmp(ip_addr, conn_udp->remote_ip_addr)) {
			return head->conn;
		} else {
			head = head->next;
		}
	}
	return NULL;
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

	struct ip_addr *ip_addr = malloc(sizeof(struct ip_addr));
	if (ip_addr == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
				"Failed in %s() on line %d.\n", __FUNCTION__,
				__LINE__);
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
