#ifdef _GNU_SOURCE
#error "Something turned it on!"
#undef _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE (200112L)
#include <string.h>

#include <transport/firefly_transport_udp_posix.h>
#include "transport/firefly_transport_udp_posix_private.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>


#include <firefly_errors.h>
#include "protocol/firefly_protocol_private.h"


#define ERROR_STR_MAX_LEN	 (256)
// TODO better buffer size
#define READ_BUFFER_SIZE	(16)


struct transport_llp *transport_llp_udp_posix_new(unsigned short local_udp_port,
	       	application_on_conn_recv_cb on_conn_recv)
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

	struct transport_llp *llp = malloc(sizeof(struct transport_llp));
	if (llp == NULL) {
		firefly_error(FIREFLY_ERROR_ALLOC, 2,
				"Failed in %s.\n", __FUNCTION__);
	}

	llp->llp_platspec = llp_udp;
	llp->conn_list = NULL;
	llp->on_conn_recv = on_conn_recv;
	return llp;
}

void transport_llp_udp_posix_free(struct transport_llp **llp)
{
	struct transport_llp_udp_posix *llp_udp =
		(struct transport_llp_udp_posix *) (*llp)->llp_platspec;
	close(llp_udp->local_udp_socket);
	struct llp_connection_list_node *head = (*llp)->conn_list;
	struct llp_connection_list_node *tmp = NULL;
	while(head != NULL) {
		tmp = head->next;
		transport_connection_udp_posix_free(&head->conn);
		free(head);
		head = tmp;
	}
	free(llp_udp->local_addr);
	free(llp_udp);
	free((*llp));
	*llp = NULL;
}

void transport_connection_udp_posix_free(struct connection **conn)
{
	struct protocol_connection_udp_posix *conn_udp =
		(struct protocol_connection_udp_posix *)
		(*conn)->transport_conn_platspec;
	free(conn_udp->remote_addr);
	free(conn_udp);
	free((*conn));
	*conn = NULL;
}

void transport_llp_udp_posix_read(struct transport_llp *llp)
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
	struct connection *conn = find_connection_by_addr(remote_addr, llp);

	if (conn == NULL) {
		struct connection *conn = malloc(sizeof(struct connection));
		struct protocol_connection_udp_posix *conn_udp = malloc(
				sizeof(struct protocol_connection_udp_posix));
		conn_udp->remote_addr = remote_addr;
		conn->transport_conn_platspec = conn_udp;
		if(llp->on_conn_recv != NULL && llp->on_conn_recv(conn)) {
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

struct connection *find_connection_by_addr(struct sockaddr_in *addr,
		struct transport_llp *llp)
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

void add_connection_to_llp(struct connection *conn, struct transport_llp *llp)
{
	struct llp_connection_list_node **head = &llp->conn_list;
	while (*head != NULL) {
		head = &(*head)->next;
	}
	*head = malloc(sizeof(struct llp_connection_list_node));
	(*head)->conn = conn;
	(*head)->next = NULL;
}
