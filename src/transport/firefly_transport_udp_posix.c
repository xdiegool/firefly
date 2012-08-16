// Must be the first include to get the right function X (TODO which one?).
#ifdef _GNU_SOURCE
#error "Something turned it on!"
#undef _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE (200112L)
#include <string.h>

#include <transport/firefly_transport_udp_posix.h>
#include "firefly_transport_udp_posix_private.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <signal.h>

#include <transport/firefly_transport.h>
#include <firefly_errors.h>
#include <eventqueue/firefly_event_queue.h>

#include "eventqueue/firefly_event_queue_private.h"
#include "protocol/firefly_protocol_private.h"
#include "transport/firefly_transport_private.h"

#define ERROR_STR_MAX_LEN      (256)
#define SCALE_BACK_NBR_DEFAULT (32)

struct firefly_transport_llp *firefly_transport_llp_udp_posix_new(
	unsigned short local_udp_port, firefly_on_conn_recv_pudp on_conn_recv)
{
	struct transport_llp_udp_posix *llp_udp;

	llp_udp             = malloc(sizeof(struct transport_llp_udp_posix));
	llp_udp->local_addr = calloc(1, sizeof(struct sockaddr_in));
	if (llp_udp == NULL || llp_udp->local_addr == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 2,
					  "Failed in %s().\n", __FUNCTION__);
	}

	llp_udp->local_addr->sin_family = AF_INET;
	llp_udp->local_addr->sin_port = htons(local_udp_port);
	llp_udp->local_addr->sin_addr.s_addr = htonl(INADDR_ANY);

	llp_udp->local_udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (llp_udp->local_udp_socket == -1) {
		char err_buf[ERROR_STR_MAX_LEN];
		strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s().\n%s\n", __FUNCTION__, err_buf);
	}
	int res = bind(llp_udp->local_udp_socket,
			(struct sockaddr *) llp_udp->local_addr,
			sizeof(struct sockaddr_in));
	if (res == -1) {
		char err_buf[ERROR_STR_MAX_LEN];
		strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
		firefly_error(FIREFLY_ERROR_LLP_BIND, 3,
				"Failed in %s().\n%s\n", __FUNCTION__, err_buf);
	}
	llp_udp->scale_back_nbr  = SCALE_BACK_NBR_DEFAULT;
	llp_udp->recv_buf_size   = 0;
	llp_udp->scale_back_size = 0;
	llp_udp->recv_buf        = NULL;
	llp_udp->on_conn_recv = on_conn_recv;

	struct firefly_transport_llp *llp = malloc(sizeof(
						struct firefly_transport_llp));
	if (llp == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 2,
					  "Failed in %s().\n", __FUNCTION__);
	}

	llp->llp_platspec = llp_udp;
	llp->conn_list = NULL;

	return llp;
}

void firefly_transport_llp_udp_posix_free(struct firefly_transport_llp **llp)
{
	struct transport_llp_udp_posix *llp_udp =
		(struct transport_llp_udp_posix *) (*llp)->llp_platspec;
	close(llp_udp->local_udp_socket);

	// Delete all connections.
	struct llp_connection_list_node *head = (*llp)->conn_list;
	struct llp_connection_list_node *tmp = NULL;
	while (head != NULL) {
		tmp = head->next;
		firefly_transport_connection_udp_posix_free_event(head->conn);
		free(head);
		head = tmp;
	}
	free(llp_udp->local_addr);
	free(llp_udp->recv_buf);
	free(llp_udp);
	free((*llp));
	*llp = NULL;
}

struct firefly_connection *firefly_connection_udp_posix_new(
				firefly_channel_is_open_f on_channel_opened,
				firefly_channel_closed_f on_channel_closed,
				firefly_channel_accept_f on_channel_recv,
				struct firefly_event_queue *event_queue,
				struct firefly_transport_llp *llp,
				struct sockaddr_in *remote_addr)
{
	struct transport_llp_udp_posix *llp_udp =
		(struct transport_llp_udp_posix *) llp->llp_platspec;
	struct protocol_connection_udp_posix *conn_udp =
		malloc(sizeof(struct protocol_connection_udp_posix));

	struct firefly_connection *conn = firefly_connection_new_register(
			on_channel_opened, on_channel_closed,
			on_channel_recv, firefly_transport_udp_posix_write, event_queue,
			conn_udp, true);
	if (conn == NULL || conn_udp == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC,
				3, "Failed in %s() on line %d.\n", __FUNCTION__,
				__LINE__);
		free(conn);
		free(conn_udp);
		return NULL;
	}
	conn_udp->remote_addr = remote_addr;
	conn_udp->socket = llp_udp->local_udp_socket;
	conn_udp->open = FIREFLY_CONNECTION_OPEN;

	return conn;
}

void firefly_transport_connection_udp_posix_free(
					struct firefly_connection *conn)
{
	struct firefly_event *ev = firefly_event_new(1,
			firefly_transport_connection_udp_posix_free_event, conn);
	conn->event_queue->offer_event_cb(conn->event_queue, ev);
}

int firefly_transport_connection_udp_posix_free_event(void *event_arg)
{
	struct firefly_connection *conn = (struct firefly_connection *) event_arg;
	struct protocol_connection_udp_posix *conn_udp =
		(struct protocol_connection_udp_posix *)
		conn->transport_conn_platspec;
	free(conn_udp->remote_addr);
	free(conn_udp);
	firefly_connection_free(&conn);
	return 0;
}

struct firefly_connection *firefly_transport_connection_udp_posix_open(
				firefly_channel_is_open_f on_channel_opened,
				firefly_channel_closed_f on_channel_closed,
				firefly_channel_accept_f on_channel_recv,
				struct firefly_event_queue *event_queue,
				const char *remote_ip_addr,
				unsigned short remote_port,
				struct firefly_transport_llp *llp)
{
	struct sockaddr_in *remote_addr = calloc(1, sizeof(struct sockaddr_in));
	if (remote_addr == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3,
				"Failed in %s() on line %d.\n", __FUNCTION__,
				__LINE__);
	}
	remote_addr->sin_family = AF_INET;
	remote_addr->sin_port = htons(remote_port);
	int res = inet_pton(AF_INET, remote_ip_addr, &remote_addr->sin_addr);
	if (res == 0) {
		firefly_error(FIREFLY_ERROR_IP_PARSE, 3,
				"Failed to convert IP address in %s() on line %d."
				"\n", __FUNCTION__, __LINE__);
	}

	struct firefly_connection *conn = firefly_connection_udp_posix_new(
			on_channel_opened, on_channel_closed, on_channel_recv,
			event_queue, llp, remote_addr);

	if (conn != NULL) {
		add_connection_to_llp(conn, llp);
	}
	return conn;
}

void firefly_transport_connection_udp_posix_close(
		struct firefly_connection *conn)
{
	struct protocol_connection_udp_posix *conn_udp =
		(struct protocol_connection_udp_posix *)
			conn->transport_conn_platspec;
	conn_udp->open = FIREFLY_CONNECTION_CLOSED;
}

// TODO refactor out this so it can be used by all transport implementations.
// Then call firefly_transport_connection_close and call a free callback?
int firefly_transport_udp_posix_clean_up(struct firefly_transport_llp *llp)
{
	int nbr_closed = 0;
	struct llp_connection_list_node **head = &llp->conn_list;
	struct protocol_connection_udp_posix *conn_udp;

	while (*head != NULL) {
		conn_udp = (struct protocol_connection_udp_posix *)
			(*head)->conn->transport_conn_platspec;
		if (!conn_udp->open) {
			firefly_transport_connection_udp_posix_free((*head)->conn);
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

void firefly_transport_udp_posix_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn)
{
	struct protocol_connection_udp_posix *conn_udp =
		(struct protocol_connection_udp_posix *)
		conn->transport_conn_platspec;
	int res = sendto(conn_udp->socket, data, data_size, 0,
			 (struct sockaddr *) conn_udp->remote_addr,
			 sizeof(*conn_udp->remote_addr));
	if (res == -1) {
		firefly_error(FIREFLY_ERROR_TRANS_WRITE, 2,
				"Failed in %s().\n", __FUNCTION__);
	}
}

static void recv_buf_resize(struct transport_llp_udp_posix *llp_udp,
						size_t pkg_len)
{
	if (llp_udp->recv_buf == NULL || pkg_len > llp_udp->recv_buf_size) {
		unsigned char *tmp;

		tmp = realloc(llp_udp->recv_buf, pkg_len); // If buf is NULL,
								// be malloc.
		if (tmp == NULL) {
			firefly_error(FIREFLY_ERROR_ALLOC, 3,
						  "Failed in %s() on line %d.\n",
						  __FUNCTION__, __LINE__);
		} else {
			llp_udp->recv_buf = tmp;
			llp_udp->recv_buf_size = pkg_len;
			llp_udp->scale_back_size = 0;
		}
		llp_udp->nbr_smaller = 0;
	} else if (llp_udp->nbr_smaller >= llp_udp->scale_back_nbr) {
		unsigned char *tmp;

		tmp = realloc(llp_udp->recv_buf, llp_udp->scale_back_size);
		if (tmp == NULL) {
			firefly_error(FIREFLY_ERROR_ALLOC, 3,
						  "Failed in %s() on line %d.\n",
						  __FUNCTION__, __LINE__);
		} else {
			llp_udp->recv_buf = tmp;
			llp_udp->recv_buf_size = llp_udp->scale_back_size;
			llp_udp->scale_back_size = 0;
		}
		llp_udp->nbr_smaller = 0;
	} else {
		llp_udp->nbr_smaller++;
		if (pkg_len > llp_udp->scale_back_size)
			llp_udp->scale_back_size = pkg_len;
	}
}

void firefly_transport_udp_posix_read(struct firefly_transport_llp *llp)
{
	struct sockaddr_in remote_addr;
	struct transport_llp_udp_posix *llp_udp =
		(struct transport_llp_udp_posix *) llp->llp_platspec;

	// Read data from socket, = 0 is crucial due to ioctl only sets the
	// first 32 bits of pkg_len
	fd_set fs;
	FD_ZERO(&fs);
	FD_SET(llp_udp->local_udp_socket, &fs);
	select(llp_udp->local_udp_socket + 1, &fs, NULL, NULL, NULL);
	size_t pkg_len = 0;
	ioctl(llp_udp->local_udp_socket, FIONREAD, &pkg_len);
	if (pkg_len == -1) {
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s() on line %d.\n", __FUNCTION__,
				__LINE__);
		pkg_len = 0;
	}
	recv_buf_resize(llp_udp, pkg_len); /* Scaleback and such. */
	socklen_t len = sizeof(struct sockaddr_in);
	int res = recvfrom(llp_udp->local_udp_socket, llp_udp->recv_buf,
			pkg_len, 0, (struct sockaddr *) &remote_addr, &len);
	if (res == -1) {
		char err_buf[ERROR_STR_MAX_LEN];
		strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s.\n%s()\n", __FUNCTION__, err_buf);
	}

	// Find existing connection or create new.
	struct firefly_connection *conn = find_connection_by_addr(&remote_addr,
									  llp);
	if (llp_udp->on_conn_recv == NULL) {
		// TODO feature consideration, on_conn_recv == NULL => silently
		// reject incomming connections.
		firefly_error(FIREFLY_ERROR_MISSING_CALLBACK, 3,
			  "Failed in %s() on line %d. 'on_conn_recv missing'\n",
			  __FUNCTION__, __LINE__);
	}
	if (conn == NULL && llp_udp->on_conn_recv != NULL) {
		char ip_addr[INET_ADDRSTRLEN];
		sockaddr_in_ipaddr(&remote_addr, ip_addr);
		conn = llp_udp->on_conn_recv(llp, ip_addr,
				sockaddr_in_port(&remote_addr));
	}
	if (conn != NULL) { // Existing, not rejected and created conn.
		protocol_data_received(conn, llp_udp->recv_buf, pkg_len);
	}
}

bool sockaddr_in_eq(struct sockaddr_in *one, struct sockaddr_in *other)
{
	return memcmp(&one->sin_port, &other->sin_port,
			sizeof(one->sin_port)) == 0 &&
		memcmp(&one->sin_addr, &other->sin_addr,
			sizeof(one->sin_addr)) == 0;

}

void sockaddr_in_ipaddr(struct sockaddr_in *addr, char *ip_addr)
{
	inet_ntop(AF_INET, &addr->sin_addr.s_addr, ip_addr, INET_ADDRSTRLEN);
}

unsigned short sockaddr_in_port(struct sockaddr_in *addr)
{
	return ntohs(addr->sin_port);
}

struct firefly_connection *find_connection_by_addr(struct sockaddr_in *ip_addr,
		struct firefly_transport_llp *llp)
{
	struct llp_connection_list_node *head = llp->conn_list;
	struct protocol_connection_udp_posix *conn_udp;

	while (head != NULL) {
		conn_udp = (struct protocol_connection_udp_posix *)
			head->conn->transport_conn_platspec;
		if (sockaddr_in_eq(conn_udp->remote_addr, ip_addr)) {
			return head->conn;
		} else {
			head = head->next;
		}
	}
	return NULL;
}

void add_connection_to_llp(struct firefly_connection *conn,
		struct firefly_transport_llp *llp)
{
	struct llp_connection_list_node *tmp = llp->conn_list;
	struct llp_connection_list_node *new_node =
		malloc(sizeof(struct llp_connection_list_node));
	new_node->conn = conn;
	new_node->next = tmp;
	llp->conn_list = new_node;
}

void firefly_transport_udp_posix_set_n_scaleback(
			struct firefly_transport_llp *llp, unsigned int nbr)
{
	struct transport_llp_udp_posix *llp_udp =
		((struct transport_llp_udp_posix *)llp->llp_platspec);
	llp_udp->scale_back_nbr = nbr;
}
