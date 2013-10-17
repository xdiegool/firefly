// Must be the first include to get the XSI-compliant version of the strerror_r
// function.
#ifdef _GNU_SOURCE
#error "Something turned it on!"
#undef _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE (200112L)
#include <pthread.h>
#include <string.h>

#include <transport/firefly_transport_tcp_posix.h>
#include "firefly_transport_tcp_posix_private.h"

#include <errno.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include <transport/firefly_transport.h>
#include <utils/firefly_errors.h>
#include <utils/firefly_event_queue.h>

#include "utils/firefly_event_queue_private.h"
#include "protocol/firefly_protocol_private.h"
#include "transport/firefly_transport_private.h"
#include "utils/cppmacros.h"

#define ERROR_STR_MAX_LEN        (256)
#define SOCK_LISTEN_BACKLOG_SIZE (10)

static bool connection_eq_sock(struct firefly_connection *conn, void *context)
{
	struct firefly_transport_connection_tcp_posix *tc_tcp;
	int socket;

	tc_tcp = conn->transport->context;
	socket = *(int *) context;

	return tc_tcp->socket == socket;
}

static void sockaddr_get_addr(struct sockaddr_in *addr, char *ip_addr)
{
	if (!inet_ntop(AF_INET, &addr->sin_addr.s_addr, ip_addr, INET_ADDRSTRLEN)) {
		char err_buf[ERROR_STR_MAX_LEN];
		strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
		firefly_error(FIREFLY_ERROR_SOCKET, 3, "Failed in %s.\n%s()\n",
					  __FUNCTION__, err_buf);
	}
}

static unsigned short sockaddr_get_port(struct sockaddr_in *addr)
{
	return ntohs(addr->sin_port);
}

struct firefly_transport_llp *firefly_transport_llp_tcp_posix_new(
		unsigned short local_tcp_port,
		firefly_on_conn_recv_ptcp on_conn_recv,
		struct firefly_event_queue *event_queue)
{
	struct firefly_transport_llp *llp;
	struct transport_llp_tcp_posix *llp_tcp;
	struct sockaddr_in *addr;
	int res;

	llp     = malloc(sizeof(*llp));
	llp_tcp = malloc(sizeof(*llp_tcp));
	addr    = calloc(1, sizeof(*addr));
	if (!llp || !llp_tcp || !addr) {
		FFL(FIREFLY_ERROR_ALLOC);
		free(addr);
		free(llp_tcp);
		free(llp);

		return NULL;
	}
	llp_tcp->local_addr = addr;

	FD_ZERO(&llp_tcp->master_set);

	llp_tcp->local_addr->sin_family      = AF_INET;
	llp_tcp->local_addr->sin_port        = htons(local_tcp_port);
	llp_tcp->local_addr->sin_addr.s_addr = htonl(INADDR_ANY);

	llp_tcp->local_tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (llp_tcp->local_tcp_socket == -1) {
		char err_buf[ERROR_STR_MAX_LEN];

		strerror_r(errno, err_buf, sizeof(err_buf));
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
					  "socket() failed in %s().\n%s\n",
					  __FUNCTION__, err_buf);

		free(llp_tcp->local_addr);
		free(llp_tcp);
		free(llp);

		return NULL;
	}

	int so_reuseaddr = 1;
	setsockopt(llp_tcp->local_tcp_socket, SOL_SOCKET, SO_REUSEADDR,
			   &so_reuseaddr, sizeof(so_reuseaddr));

	FD_SET(llp_tcp->local_tcp_socket, &llp_tcp->master_set);
	llp_tcp->max_sock = llp_tcp->local_tcp_socket;

	res = bind(llp_tcp->local_tcp_socket,
			   (struct sockaddr *) llp_tcp->local_addr,
			   sizeof(struct sockaddr_in));
	if (res == -1) {
		char err_buf[ERROR_STR_MAX_LEN];

		strerror_r(errno, err_buf, sizeof(err_buf));
		firefly_error(FIREFLY_ERROR_LLP_BIND, 3,
					  "bind() failed in %s().\n%s\n",
					  __func__, err_buf);

		close(llp_tcp->local_tcp_socket);
		free(llp_tcp->local_addr);
		free(llp_tcp);
		free(llp);

		return NULL;
	}

	res = listen(llp_tcp->local_tcp_socket, SOCK_LISTEN_BACKLOG_SIZE);
	if (res == -1) {
		char err_buf[ERROR_STR_MAX_LEN];

		strerror_r(errno, err_buf, sizeof(err_buf));
		firefly_error(FIREFLY_ERROR_SOCKET, 4,
					  "listen() failed in %s():%d.\n%s\n",
					  __func__, __LINE__, err_buf);

		close(llp_tcp->local_tcp_socket);
		free(llp_tcp->local_addr);
		free(llp_tcp);
		free(llp);

		return NULL;
	}

	llp_tcp->on_conn_recv          = on_conn_recv;
	llp_tcp->event_queue           = event_queue;
	llp->llp_platspec              = llp_tcp;
	llp->conn_list                 = NULL;
	llp->protocol_data_received_cb = protocol_data_received;
	llp->state                     = FIREFLY_LLP_OPEN;

	return llp;
}

static void check_llp_free(struct firefly_transport_llp *llp)
{
	if (llp->state == FIREFLY_LLP_CLOSING && llp->conn_list == NULL) {
		struct transport_llp_tcp_posix *llp_tcp;
		int res;

		llp_tcp = llp->llp_platspec;

		res = close(llp_tcp->local_tcp_socket);
		if (res) {
			char err_buf[ERROR_STR_MAX_LEN];

			strerror_r(errno, err_buf, sizeof(err_buf));
			firefly_error(FIREFLY_ERROR_SOCKET, 4,
						  "Failed to close() socket in %s():%d.\n%s\n",
						  __func__, __LINE__, err_buf);
		}
		free(llp_tcp->local_addr);
		free(llp_tcp);
		free(llp);
	}
}

static int free_event(void *event_arg)
{
	struct firefly_transport_llp *llp;
	struct llp_connection_list_node *head;

	llp = event_arg;

	llp->state = FIREFLY_LLP_CLOSING;

	// Close all connections.
	head = llp->conn_list;
	while (head != NULL) {
		firefly_connection_close(head->conn);
		head = head->next;
	}
	check_llp_free(llp);

	return 0;
}


void firefly_transport_llp_tcp_posix_free(struct firefly_transport_llp *llp)
{
	struct transport_llp_tcp_posix *llp_tcp;
	struct firefly_event_queue *eq;
	int ret;

	llp_tcp = llp->llp_platspec;
	eq      = llp_tcp->event_queue;

	ret = eq->offer_event_cb(eq, FIREFLY_PRIORITY_LOW, free_event, llp, 0, NULL);

	FFLIF(ret < 0, FIREFLY_ERROR_EVENT);
}

static int connection_open(struct firefly_connection *conn)
{
	struct firefly_transport_connection_tcp_posix *tcup;

	tcup = conn->transport->context;
	add_connection_to_llp(conn, tcup->llp);

	return 0;
}

static int connection_close(struct firefly_connection *conn)
{
	struct firefly_transport_llp *llp;
	struct firefly_transport_connection_tcp_posix *tcup;

	tcup = conn->transport->context;
	llp = tcup->llp;

	remove_connection_from_llp(tcup->llp, conn, firefly_connection_eq_ptr);
	close(tcup->socket);
	free(tcup->remote_addr);
	free(conn->transport);
	free(tcup);
	check_llp_free(llp);

	return 0;
}

struct firefly_transport_connection *firefly_transport_connection_tcp_posix_new(
		struct firefly_transport_llp *llp,
		int existing_socket,
		const char *remote_ipaddr,
		unsigned short remote_port)
{
	struct firefly_transport_connection *tc;
	struct firefly_transport_connection_tcp_posix *tcup;
	struct transport_llp_tcp_posix *llp_tcp;
	struct sockaddr_in *remote_addr;
	int res;

	llp_tcp     = llp->llp_platspec;
	tc          = malloc(sizeof(*tc));
	tcup        = malloc(sizeof(*tcup));
	remote_addr = calloc(1, sizeof(*remote_addr));
	if (tc == NULL || tcup == NULL || remote_addr == NULL) {
		free(tc);
		free(tcup);
		free(remote_addr);

		return NULL;
	}

	tcup->remote_addr             = remote_addr;
	tcup->remote_addr->sin_family = AF_INET;
	tcup->remote_addr->sin_port   = htons(remote_port);
	res = inet_pton(AF_INET, remote_ipaddr, &tcup->remote_addr->sin_addr);
	if (res != 1) {
		FFL(FIREFLY_ERROR_IP_PARSE);

		free(tcup->remote_addr);
		free(tc);
		free(tcup);

		return NULL;
	}

	if (existing_socket == -1) {
		tcup->socket = socket(AF_INET, SOCK_STREAM, 0);
		if (tcup->socket == -1) {
			char err_buf[ERROR_STR_MAX_LEN];

			strerror_r(errno, err_buf, sizeof(err_buf));
			firefly_error(FIREFLY_ERROR_TRANS_WRITE, 4,
						  "socket() failed in %s():%d.\n%s\n",
						  __func__, __LINE__, err_buf);
		}

		FD_SET(tcup->socket, &llp_tcp->master_set);
		if (tcup->socket > llp_tcp->max_sock) {
			llp_tcp->max_sock = tcup->socket;
		}

		res = connect(tcup->socket, (struct sockaddr *) tcup->remote_addr,
					  sizeof(*tcup->remote_addr));
		if (res == -1) {
			char err_buf[ERROR_STR_MAX_LEN];

			strerror_r(errno, err_buf, sizeof(err_buf));
			firefly_error(FIREFLY_ERROR_SOCKET, 4,
						  "connect() failed in %s():%d.\n%s\n",
						  __func__, __LINE__, err_buf);
			return NULL;
		}
	} else {
		tcup->socket = existing_socket;
	}

	tcup->llp     = llp;
	tc->context   = tcup;
	tc->open      = connection_open;
	tc->close     = connection_close;
	tc->write     = firefly_transport_tcp_posix_write;
	tc->ack       = NULL;

	return tc;
}

void firefly_transport_tcp_posix_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id)
{
	struct firefly_transport_connection_tcp_posix *conn_tcp;
	int res;

	// Don't need these in TCP
	UNUSED_VAR(important);
	UNUSED_VAR(id);

	conn_tcp = conn->transport->context;
	res      = send(conn_tcp->socket, data, data_size, 0);
	if (res == -1) {
		char err_buf[ERROR_STR_MAX_LEN];

		strerror_r(errno, err_buf, sizeof(err_buf));
		firefly_error(FIREFLY_ERROR_TRANS_WRITE, 4,
					  "send() failed in %s():%d.\n%s\n",
					  __func__, __LINE__, err_buf);
		firefly_connection_raise_later(conn, FIREFLY_ERROR_TRANS_WRITE,
									   "Failed to send() data");
	}
}

static void *firefly_transport_tcp_posix_read_run(void *args)
{
	struct firefly_transport_llp *llp;

	llp = args;

	while (true)
		firefly_transport_tcp_posix_read(llp);

	return NULL;
}

int firefly_transport_tcp_posix_run(struct firefly_transport_llp *llp)
{
	int res;
	struct transport_llp_tcp_posix *llp_tcp;

	res     = 0;
	llp_tcp = llp->llp_platspec;

	res = pthread_create(&llp_tcp->read_thread, NULL,
						 firefly_transport_tcp_posix_read_run, llp);

	return res;
}

int firefly_transport_tcp_posix_stop(struct firefly_transport_llp *llp)
{
	struct transport_llp_tcp_posix *llp_tcp;
	int res;

	llp_tcp = llp->llp_platspec;
	res     = 0;

	// TODO: get rid of pthread_cancel, not a nice way to stop threads with and
	// can lead to memory not being cleaned up.
	if (!(res = pthread_cancel(llp_tcp->read_thread))) {
		return res;
	}
	res = pthread_join(llp_tcp->read_thread, NULL);

	return res;
}

struct firefly_event_llp_read_tcp_posix {
	struct firefly_transport_llp *llp;
	struct sockaddr_in addr;
	int socket;
	size_t len;
	unsigned char *data;
};

static int read_event(void *event_arg)
{
	struct firefly_event_llp_read_tcp_posix *ev_arg;
	struct transport_llp_tcp_posix *llp_tcp;
	struct firefly_connection *conn;
	struct firefly_event_queue *eq;
	firefly_on_conn_recv_ptcp on_conn_recv;

	ev_arg       = event_arg;
	llp_tcp      = ev_arg->llp->llp_platspec;
	eq           = llp_tcp->event_queue;
	on_conn_recv = llp_tcp->on_conn_recv;

	// Find existing connection.
	conn = find_connection(ev_arg->llp, &ev_arg->socket, connection_eq_sock);
	if (conn != NULL)
		ev_arg->llp->protocol_data_received_cb(conn, ev_arg->data, ev_arg->len);

	free(ev_arg);

	return 0;
}

void firefly_transport_tcp_posix_read(struct firefly_transport_llp *llp)
{
	struct transport_llp_tcp_posix *llp_tcp;
	size_t pkg_len;
	struct firefly_event_queue *eq;
	firefly_on_conn_recv_ptcp on_conn_recv;
	fd_set fs;
	int res;

	llp_tcp      = llp->llp_platspec;
	pkg_len      = 0; /* ioctl() sets only the lower 32 bit. */
	eq           = llp_tcp->event_queue;
	on_conn_recv = llp_tcp->on_conn_recv;

	do {
		FD_ZERO(&fs); // Probably unnecessary 'cause memcpy but better safe than sorry.
		memcpy(&fs, &llp_tcp->master_set, sizeof(llp_tcp->master_set));
		res = select(llp_tcp->max_sock + 1, &fs, NULL, NULL, NULL);
	} while (res == -1 && errno == EINTR);

	for (int i = 0; i <= llp_tcp->max_sock; i++) {
		int sock;
		struct sockaddr_in remote_addr;
		socklen_t len;
		int64_t eid;
		struct firefly_event_llp_read_tcp_posix *ev_arg;

		eid      = -1;
		sock     = -1;
		len      = sizeof(remote_addr);

		// Check if descriptor is in set that was ready:
		if (FD_ISSET(i, &fs)) {
			// Check if there was activity on listen socket:
			if (i == llp_tcp->local_tcp_socket) {
				sock = accept(llp_tcp->local_tcp_socket,
							  (struct sockaddr *) &remote_addr, &len);

				FD_SET(sock, &llp_tcp->master_set);
				if (sock > llp_tcp->max_sock) {
					llp_tcp->max_sock = sock;
				}

				unsigned short port = sockaddr_get_port(&remote_addr);
				char ip[INET_ADDRSTRLEN];
				sockaddr_get_addr(&remote_addr, ip);
				eid = on_conn_recv ? on_conn_recv(llp, sock, ip, port) : -1;
			}

			// Try to read data
			sock = sock != -1 ? sock : i;

			res = ioctl(sock, FIONREAD, &pkg_len);
			if (res == -1) {
				char err_buf[ERROR_STR_MAX_LEN];
				strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
				firefly_error(FIREFLY_ERROR_SOCKET, 3,
							  "ioctl() failed in %s().\n%s\n",
							  __FUNCTION__, err_buf);

				pkg_len = 0;
			}
			if (pkg_len == 0) // If there's nothing to read, continue loop
				continue;

			ev_arg = malloc(sizeof(*ev_arg) + pkg_len);
			if (!ev_arg) {
				FFL(FIREFLY_ERROR_ALLOC);
				return;
			}
			ev_arg->data = malloc(pkg_len);
			if (!ev_arg->data) {
				FFL(FIREFLY_ERROR_ALLOC);
				free(ev_arg);
				return;
			}

			res = recv(sock, ev_arg->data, pkg_len, 0);
			if (res == -1) {
				char err_buf[ERROR_STR_MAX_LEN];
				strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
				firefly_error(FIREFLY_ERROR_SOCKET, 4,
							  "recv() on socket %d failed in %s().\n%s\n",
							  sock, __FUNCTION__, err_buf);

				return;
			}

			res = getpeername(sock, (struct sockaddr *) &remote_addr, &len);
			if (res == -1) {
				char err_buf[ERROR_STR_MAX_LEN];
				strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
				firefly_error(FIREFLY_ERROR_SOCKET, 4,
							  "getpeername() failed in %s():%d:\n%s\n",
							  __func__, __LINE__, err_buf);

				return;
			}

			ev_arg->llp    = llp;
			ev_arg->socket = sock;
			ev_arg->len    = pkg_len;
			ev_arg->addr   = remote_addr;
			/* Member 'data' already filled in recvfrom(). */

			if (eid != -1) {
				eq->offer_event_cb(eq, FIREFLY_PRIORITY_HIGH, read_event,
								   ev_arg, 1, &eid);
			} else {
				eq->offer_event_cb(eq, FIREFLY_PRIORITY_HIGH, read_event,
								   ev_arg, 0, NULL);
			}
		}
	}
}
