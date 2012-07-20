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

#include <transport/firefly_transport.h>
#include <firefly_errors.h>

#include "protocol/firefly_protocol_private.h"
#include "transport/firefly_transport_private.h"

#define ERROR_STR_MAX_LEN	 (256)
#define READ_BUFFER_SIZE	(16) // TODO better buffer size

struct firefly_transport_llp *firefly_transport_llp_udp_posix_new(unsigned short local_udp_port,
	       	firefly_on_conn_recv on_conn_recv)
{
	struct transport_llp_udp_posix *llp_udp =
	       	malloc(sizeof(struct transport_llp_udp_posix));
	if (llp_udp == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 2,
				"Failed in %s.\n", __FUNCTION__);
	}
	llp_udp->local_addr = calloc(1, sizeof(struct sockaddr_in));
	if (llp_udp->local_addr == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 2,
				"Failed in %s.\n", __FUNCTION__);
	}

	llp_udp->local_addr->sin_family = AF_INET;
	llp_udp->local_addr->sin_port = htons(local_udp_port);
	llp_udp->local_addr->sin_addr.s_addr = htonl(INADDR_ANY);

	llp_udp->local_udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (llp_udp->local_udp_socket == -1) {
		char err_buf[ERROR_STR_MAX_LEN];
		strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s.\n%s\n", __FUNCTION__, err_buf);
	}
	int res = bind(llp_udp->local_udp_socket,
			(struct sockaddr *) llp_udp->local_addr,
			sizeof(struct sockaddr_in));
	if (res == -1) {
		char err_buf[ERROR_STR_MAX_LEN];
		strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s.\n%s\n", __FUNCTION__, err_buf);
	}

	struct firefly_transport_llp *llp = malloc(sizeof(struct firefly_transport_llp));
	if (llp == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 2,
				"Failed in %s.\n", __FUNCTION__);
	}

	llp->llp_platspec = llp_udp;
	llp->conn_list = NULL;
	llp->on_conn_recv = on_conn_recv;
	return llp;
}

void firefly_transport_udp_posix_free(struct firefly_transport_llp **llp)
{
	struct transport_llp_udp_posix *llp_udp =
		(struct transport_llp_udp_posix *) (*llp)->llp_platspec;
	close(llp_udp->local_udp_socket);
	struct llp_connection_list_node *head = (*llp)->conn_list;
	struct llp_connection_list_node *tmp = NULL;
	while(head != NULL) {
		tmp = head->next;
		firefly_transport_connection_udp_posix_free(&head->conn);
		free(head);
		head = tmp;
	}
	free(llp_udp->local_addr);
	free(llp_udp);
	free((*llp));
	*llp = NULL;
}

void firefly_transport_connection_udp_posix_free(struct firefly_connection **conn)
{
	struct protocol_connection_udp_posix *conn_udp =
		(struct protocol_connection_udp_posix *)
		(*conn)->transport_conn_platspec;
	free(conn_udp->remote_addr);
	free(conn_udp);
	free((*conn));
	*conn = NULL;
}

struct firefly_connection *firefly_transport_connection_udp_posix_open(char *ip_addr,
		unsigned short port, struct firefly_transport_llp *llp)
{
	struct firefly_connection *conn = malloc(sizeof(struct firefly_connection));
	if (conn == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 2, "Failed in %s on line %d.\n",
					  __FUNCTION__, __LINE__);
	}
	struct protocol_connection_udp_posix *conn_udp =
		malloc(sizeof(struct protocol_connection_udp_posix));
	if (conn_udp == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3, "Failed in %s on line %d.\n",
					  __FUNCTION__, __LINE__);
	}
	conn_udp->remote_addr = calloc(1, sizeof(struct sockaddr_in));
	if (conn_udp->remote_addr == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 3, "Failed in %s on line %d.\n",
					  __FUNCTION__, __LINE__);
	}

	conn_udp->remote_addr->sin_family = AF_INET;
	conn_udp->remote_addr->sin_port = htons(port);
	int res = inet_pton(AF_INET, ip_addr, &conn_udp->remote_addr->sin_addr);
	if (res == 0) {
		firefly_error(FIREFLY_ERROR_SOCKET, 3, "Failed in %s on line %d.\n",
					  __FUNCTION__, __LINE__);
	}

	conn_udp->socket = ((struct transport_llp_udp_posix *)
			llp->llp_platspec)->local_udp_socket;
	conn->transport_conn_platspec = conn_udp;

	add_connection_to_llp(conn, llp);
	return conn;
}

void firefly_transport_write_udp_posix(unsigned char *data, size_t data_size,
		struct firefly_connection *conn)
{
	struct protocol_connection_udp_posix *conn_udp = (struct
			protocol_connection_udp_posix *)
				conn->transport_conn_platspec;
	int res = sendto(conn_udp->socket, data, data_size, 0, (struct sockaddr
				*) conn_udp->remote_addr,
			sizeof(*conn_udp->remote_addr));
	if (res == -1) {
		firefly_error(FIREFLY_ERROR_SOCKET, 2,
				"Failed in %s.\n", __FUNCTION__);
	}
}

void firefly_transport_udp_posix_read(struct firefly_transport_llp *llp)
{
	struct sockaddr_in *remote_addr = malloc(sizeof(struct sockaddr_in));
	struct transport_llp_udp_posix *llp_udp =
		(struct transport_llp_udp_posix *) llp->llp_platspec;

	// Read data from socket
	socklen_t len = sizeof(struct sockaddr_in);
	unsigned char buf[READ_BUFFER_SIZE];
	int res = recvfrom(llp_udp->local_udp_socket, buf, READ_BUFFER_SIZE, 0,
			(struct sockaddr *) remote_addr, &len);
	if (res == -1) {
		char err_buf[ERROR_STR_MAX_LEN];
		strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
				"Failed in %s.\n%s\n", __FUNCTION__, err_buf);
	}

	// Find existing connection or create new
	struct firefly_connection *conn = find_connection_by_addr(remote_addr,
									llp);

	if (conn == NULL) {
		struct firefly_connection *conn =
			malloc(sizeof(struct firefly_connection));
		struct protocol_connection_udp_posix *conn_udp =
			malloc(sizeof(struct protocol_connection_udp_posix));
		conn_udp->remote_addr = remote_addr;
		conn->transport_conn_platspec = conn_udp;
		if(llp->on_conn_recv != NULL && llp->on_conn_recv(conn)) {
			conn_udp->socket = llp_udp->local_udp_socket;
			add_connection_to_llp(conn, llp);
			protocol_data_received(conn, buf, READ_BUFFER_SIZE);
		} else {
			free(conn_udp);
			free(conn);
			free(remote_addr);
		}
	} else {
		protocol_data_received(conn, buf, READ_BUFFER_SIZE);
		free(remote_addr);
	}
}

bool sockaddr_in_eq(struct sockaddr_in *one, struct sockaddr_in *other)
{
	return memcmp(&one->sin_port, &other->sin_port,
			sizeof(one->sin_port)) == 0 &&
		memcmp(&one->sin_addr, &other->sin_addr,
			sizeof(one->sin_addr)) == 0;

}

struct firefly_connection *find_connection_by_addr(struct sockaddr_in *addr,
		struct firefly_transport_llp *llp)
{
	struct llp_connection_list_node *head = llp->conn_list;
	struct protocol_connection_udp_posix *conn_udp;

	while (head != NULL) {
		conn_udp = (struct protocol_connection_udp_posix *)
			head->conn->transport_conn_platspec;
		if (sockaddr_in_eq(conn_udp->remote_addr, addr)) {
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
