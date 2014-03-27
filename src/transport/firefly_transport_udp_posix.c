// Must be the first include to get the XSI-compliant version of the strerror_r
// function.
#ifdef _GNU_SOURCE
#error "Something turned it on!"
#undef _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE (200112L)
#include <pthread.h>
#include <string.h>

#include <transport/firefly_transport_udp_posix.h>
#include "firefly_transport_udp_posix_private.h"
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#ifdef LABCOMM_COMPAT

#include <selectLib.h>
#include <sockLib.h>
#include <inetLib.h>
#include <ioLib.h>
#include <taskLib.h>
#include <utils/firefly_resend_vx.h>

#else

#include <sys/select.h>
#include <utils/firefly_resend_posix.h>

#endif

#include <signal.h>

#include <transport/firefly_transport.h>
#include <utils/firefly_errors.h>
#include <utils/firefly_event_queue.h>

#include "utils/firefly_event_queue_private.h"
#include "protocol/firefly_protocol_private.h"
#include "transport/firefly_transport_private.h"
#include "utils/cppmacros.h"

#define ERROR_STR_MAX_LEN      (256)
#define SCALE_BACK_NBR_DEFAULT (32)

struct firefly_transport_llp *firefly_transport_llp_udp_posix_new(
	unsigned short local_udp_port,
	firefly_on_conn_recv_pudp on_conn_recv,
	struct firefly_event_queue *event_queue)
{
	struct firefly_transport_llp *llp;
	struct transport_llp_udp_posix *llp_udp;
	void *tmp;
	int res;

	llp     = malloc(sizeof(*llp));
	llp_udp = malloc(sizeof(*llp_udp));
	tmp     = calloc(1, sizeof(struct sockaddr_in));
	if (!llp || !llp_udp | !tmp) {
		FFL(FIREFLY_ERROR_ALLOC);
		free(tmp);
		free(llp_udp);
		free(llp);

		return NULL;
	}
	llp_udp->local_addr = tmp;

	llp_udp->local_addr->sin_family = AF_INET;
	llp_udp->local_addr->sin_port = htons(local_udp_port);
	llp_udp->local_addr->sin_addr.s_addr = htonl(INADDR_ANY);

	llp_udp->local_udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (llp_udp->local_udp_socket == -1) {
		char err_buf[ERROR_STR_MAX_LEN];

#ifdef LABCOMM_COMPAT
		err_buf[0] = '\0';
#else
		strerror_r(errno, err_buf, sizeof(err_buf));
#endif
		firefly_error(FIREFLY_ERROR_SOCKET, 3,
			      "socket() failed in %s().\n%s\n",
			      __FUNCTION__, err_buf);
		close(llp_udp->local_udp_socket);
		free(llp_udp->local_addr);
		free(llp_udp);
		free(llp);

		return NULL;
	}
	res = bind(llp_udp->local_udp_socket,
		   (struct sockaddr *) llp_udp->local_addr,
		   sizeof(struct sockaddr_in));
	if (res == -1) {
		char err_buf[ERROR_STR_MAX_LEN];

#ifdef LABCOMM_COMPAT
		err_buf[0] = '\0';
#else
		strerror_r(errno, err_buf, sizeof(err_buf));
#endif
		firefly_error(FIREFLY_ERROR_LLP_BIND, 3,
			      "bind() failed in %s().\n%s\n",
			      __FUNCTION__, err_buf);
		close(llp_udp->local_udp_socket);
		free(llp_udp->local_addr);
		free(llp_udp);
		free(llp);

		return NULL;
	}
	llp_udp->on_conn_recv = on_conn_recv;
	llp_udp->event_queue = event_queue;
	llp_udp->resend_queue = firefly_resend_queue_new();

	llp->llp_platspec = llp_udp;
	llp->conn_list = NULL;
	llp->protocol_data_received_cb = protocol_data_received;
	llp->state = FIREFLY_LLP_OPEN;

	return llp;
}

void firefly_transport_llp_udp_posix_free(struct firefly_transport_llp *llp)
{
	struct transport_llp_udp_posix *llp_udp;
	int ret;

	llp_udp = llp->llp_platspec;

	ret = llp_udp->event_queue->offer_event_cb(llp_udp->event_queue,
			FIREFLY_PRIORITY_LOW,
			firefly_transport_llp_udp_posix_free_event,
			llp, 0, NULL);
	FFLIF(ret < 0, FIREFLY_ERROR_ALLOC);
}

static void check_llp_free(struct firefly_transport_llp *llp)
{
	struct transport_llp_udp_posix *llp_udp;
	if (llp->state == FIREFLY_LLP_CLOSING && llp->conn_list == NULL) {
		llp_udp = llp->llp_platspec;
		close(llp_udp->local_udp_socket);
		free(llp_udp->local_addr);
		firefly_resend_queue_free(llp_udp->resend_queue);
		free(llp_udp);
		free(llp);
	}
}

int firefly_transport_llp_udp_posix_free_event(void *event_arg)
{
	struct firefly_transport_llp *llp;
	llp = event_arg;

	llp->state = FIREFLY_LLP_CLOSING;

	// Close all connections.
	struct llp_connection_list_node *head = llp->conn_list;
	while (head != NULL) {
		firefly_connection_close(head->conn);
		head = head->next;
	}
	check_llp_free(llp);
	return 0;
}

static int connection_open(struct firefly_connection *conn)
{
	struct firefly_transport_connection_udp_posix *tcup;
	tcup = conn->transport->context;
	add_connection_to_llp(conn, tcup->llp);
	return 0;
}

static int connection_close(struct firefly_connection *conn)
{
	struct firefly_transport_llp *llp;
	struct firefly_transport_connection_udp_posix *tcup;
	tcup = conn->transport->context;
	llp = tcup->llp;

	remove_connection_from_llp(tcup->llp, conn,
			firefly_connection_eq_ptr);
	free(tcup->remote_addr);
	free(conn->transport);
	free(tcup);
	check_llp_free(llp);
	return 0;
}

struct firefly_transport_connection *firefly_transport_connection_udp_posix_new(
		struct firefly_transport_llp *llp,
		const char *remote_ipaddr,
		unsigned short remote_port,
		unsigned int timeout)
{
	struct firefly_transport_connection *tc;
	struct firefly_transport_connection_udp_posix *tcup;
	struct transport_llp_udp_posix *llp_udp = llp->llp_platspec;
	int res;
	tc = malloc(sizeof(*tc));
	tcup = malloc(sizeof(*tcup));
	if (tc == NULL || tcup == NULL) {
		free(tc);
		free(tcup);
		return NULL;
	}
	tcup->remote_addr = calloc(1, sizeof(*tcup->remote_addr));
	if (tcup->remote_addr == NULL) {
		free(tc);
		free(tcup);
		return NULL;
	}
	tcup->remote_addr->sin_family = AF_INET;
	tcup->remote_addr->sin_port = htons(remote_port);
	res = inet_pton(AF_INET, remote_ipaddr, &tcup->remote_addr->sin_addr);
	if (res == 0) {
		FFL(FIREFLY_ERROR_IP_PARSE);
		free(tcup->remote_addr);
		free(tc);
		free(tcup);
		return NULL;
	}
	tcup->socket = llp_udp->local_udp_socket;
	tcup->llp = llp;
	tcup->timeout = timeout;
	tc->context = tcup;
	tc->open = connection_open;
	tc->close = connection_close;
	tc->write = firefly_transport_udp_posix_write;
	tc->ack = firefly_transport_udp_posix_ack;
	return tc;
}

void firefly_transport_udp_posix_ack(unsigned char pkt_id,
		struct firefly_connection *conn)
{
	struct firefly_transport_connection_udp_posix *conn_udp;
	struct transport_llp_udp_posix *llpup;

	conn_udp = conn->transport->context;
	llpup = conn_udp->llp->llp_platspec;
	firefly_resend_remove(llpup->resend_queue, pkt_id);
}

void firefly_transport_udp_posix_write(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id)
{
	struct firefly_transport_connection_udp_posix *conn_udp;
	int res;

	conn_udp = conn->transport->context;
	res = sendto(conn_udp->socket, (void *) data, data_size, 0,
		     (struct sockaddr *) conn_udp->remote_addr,
		     sizeof(*conn_udp->remote_addr));
	if (res == -1) {
		firefly_error(FIREFLY_ERROR_TRANS_WRITE, 1, "sendto() failed");
		firefly_connection_raise_later(conn,
				FIREFLY_ERROR_TRANS_WRITE, "sendto() failed");
	}
	if (important) {
		unsigned char *new_data;
		struct transport_llp_udp_posix *llp_ps;

		if (!id) {
			firefly_error(FIREFLY_ERROR_TRANS_WRITE, 1,
					"Parameter id was NULL.\n");
			return;
		}
		new_data = malloc(data_size);
		if (!new_data) {
			FFL(FIREFLY_ERROR_ALLOC);
			return;
		}
		memcpy(new_data, data, data_size);
		llp_ps = conn_udp->llp->llp_platspec;
		*id = firefly_resend_add(llp_ps->resend_queue,
				new_data, data_size, conn_udp->timeout,
				FIREFLY_TRANSPORT_UDP_POSIX_DEFAULT_RETRIES, conn);
	}
}

void *firefly_transport_udp_posix_read_run(void *args)
{
	struct firefly_transport_llp *llp;

	llp = args;
	while (true)
		firefly_transport_udp_posix_read(llp);

	return NULL;
}

static void resend_on_no_ack(struct firefly_connection *conn)
{
	firefly_connection_raise_later(conn, FIREFLY_ERROR_TRANS_WRITE, NULL);
}

int firefly_transport_udp_posix_run(struct firefly_transport_llp *llp)
{
	int res;
	struct transport_llp_udp_posix *llp_udp;
	struct firefly_resend_loop_args *largs;

	llp_udp = llp->llp_platspec;
	largs = malloc(sizeof(*largs));
	if (!largs)
		return -1;
	largs->rq = llp_udp->resend_queue;
	largs->on_no_ack = resend_on_no_ack;

	/* TODO: Clean this up. */
#ifndef LABCOMM_COMPAT
	res = pthread_create(&llp_udp->read_thread, NULL,
			firefly_transport_udp_posix_read_run, llp);
	if (res < 0)
		goto fail;
	res = pthread_create(&llp_udp->resend_thread, NULL,
				 firefly_resend_run, largs);
	if (res < 0)
		goto ptfail;
	return 0;
 ptfail:
	pthread_cancel(llp_udp->read_thread);
	goto fail;
#else
	res = taskSpawn("ff_read_thread", 254, 0, 20000,
					firefly_transport_udp_posix_read_run,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0); /* TODO: arg */
	if (res == ERROR)
		goto fail;
	llp_udp->tid_read = res;
	res = taskSpawn("ff_resend_thread", 254, 0, 20000,
					firefly_resend_run,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0); /* TODO: arg */
	if (res == ERROR)
		goto vxfail;
	llp_udp->tid_resend = res;
	return 0;
 vxfail:
	taskDelete(llp_udp->tid_read);
	goto fail;
#endif
 fail:
	free(largs);
	return res;
}

int firefly_transport_udp_posix_stop(struct firefly_transport_llp *llp)
{
	struct transport_llp_udp_posix *llp_udp;

	llp_udp = llp->llp_platspec;

#ifndef LABCOMM_COMPAT
	pthread_cancel(llp_udp->read_thread);
	pthread_cancel(llp_udp->resend_thread);
	pthread_join(llp_udp->resend_thread, NULL);
	pthread_join(llp_udp->read_thread, NULL);
#else
	taskDelete(llp_udp->tid_read);
	taskDelete(llp_udp->tid_resend);
#endif
	return 0;
}

struct firefly_event_llp_read_udp_posix {
	struct firefly_transport_llp *llp;
	struct sockaddr_in addr;
	size_t len;
	unsigned char *data;
};

static int firefly_transport_udp_posix_read_event(void *event_arg)
{
	struct firefly_event_llp_read_udp_posix *ev_arg;
	struct transport_llp_udp_posix *llp_udp;
	struct firefly_connection *conn;

	ev_arg = event_arg;
	llp_udp = ev_arg->llp->llp_platspec;

	// Find existing connection or create new.
	conn = find_connection(ev_arg->llp, &ev_arg->addr, connection_eq_inaddr);
	if (conn == NULL) {
		char ip_addr[INET_ADDRSTRLEN];
		sockaddr_in_ipaddr(&ev_arg->addr, ip_addr);
		int64_t ev_id = 0;
		if (llp_udp->on_conn_recv != NULL &&
				(ev_id = llp_udp->on_conn_recv(ev_arg->llp, ip_addr,
					sockaddr_in_port(&ev_arg->addr))) > 0) {
			return llp_udp->event_queue->offer_event_cb(llp_udp->event_queue,
					FIREFLY_PRIORITY_HIGH,
					firefly_transport_udp_posix_read_event,
					ev_arg, 1, &ev_id);
		} else {
			free(ev_arg->data);
		}
	} else {
		ev_arg->llp->protocol_data_received_cb(conn, ev_arg->data, ev_arg->len);
	}
	free(ev_arg);
	return 0;
}

void firefly_transport_udp_posix_read(struct firefly_transport_llp *llp)
{
	struct transport_llp_udp_posix *llp_udp;
	fd_set fs;
	int res;
	size_t pkg_len = 0;	/* ioctl() sets only the lower 32 bit. */
	struct sockaddr_in remote_addr;
	struct firefly_event_llp_read_udp_posix *ev_arg;
	socklen_t len;

	llp_udp = llp->llp_platspec;
	do {
		FD_ZERO(&fs);
		FD_SET(llp_udp->local_udp_socket, &fs);
		res = select(llp_udp->local_udp_socket + 1, &fs, NULL, NULL, NULL);
	} while (res == -1 && errno == EINTR);
	if (res == -1) {
		if (errno == ENOMEM) {
			firefly_error(FIREFLY_ERROR_ALLOC, 1, "select()");
		} else {
			firefly_error(FIREFLY_ERROR_SOCKET, 2,
				      "select() ret unspecified %d", res);
		}
		return;
	}
#ifdef LABCOMM_COMPAT
	res = ioctl(llp_udp->local_udp_socket, FIONREAD, pkg_len);
#else
	res = ioctl(llp_udp->local_udp_socket, FIONREAD, &pkg_len);
#endif
	if (res == -1) {
		FFL(FIREFLY_ERROR_SOCKET);
		pkg_len = 0;
	}
	ev_arg = malloc(sizeof(*ev_arg));
	if (!ev_arg) {
		FFL(FIREFLY_ERROR_ALLOC);
		free(ev_arg);
		return;
	}
	ev_arg->data = malloc(pkg_len);
	if (!ev_arg->data) {
		FFL(FIREFLY_ERROR_ALLOC);
		free(ev_arg);
		return;
	}
	len = sizeof(remote_addr);
	res = recvfrom(llp_udp->local_udp_socket, (void *) ev_arg->data, pkg_len, 0,
		       (struct sockaddr *) &remote_addr, (void *) &len);
	if (res == -1) {
		char err_buf[ERROR_STR_MAX_LEN];
#ifdef LABCOMM_COMPAT
		err_buf[0] = '\0';
#else
		strerror_r(errno, err_buf, ERROR_STR_MAX_LEN);
#endif
		firefly_error(FIREFLY_ERROR_SOCKET, 3, "Failed in %s.\n%s()\n",
			      __FUNCTION__, err_buf);
	}
	ev_arg->llp	= llp;
	ev_arg->addr	= remote_addr;
	ev_arg->len	= pkg_len;
	/* Member 'data' already filled in recvfrom(). */

	llp_udp->event_queue->offer_event_cb(llp_udp->event_queue,
			FIREFLY_PRIORITY_HIGH,
			firefly_transport_udp_posix_read_event,
			ev_arg, 0, NULL);
}

bool sockaddr_in_eq(struct sockaddr_in *one, struct sockaddr_in *other)
{
	return memcmp(&one->sin_port, &other->sin_port,
			sizeof(one->sin_port)) == 0 &&
		memcmp(&one->sin_addr, &other->sin_addr,
			sizeof(one->sin_addr)) == 0;
}

bool connection_eq_inaddr(struct firefly_connection *conn, void *context)
{
	return sockaddr_in_eq(((struct firefly_transport_connection_udp_posix *)
				conn->transport->context)->remote_addr, (struct sockaddr_in
					*) context);
}

void sockaddr_in_ipaddr(struct sockaddr_in *addr, char *ip_addr)
{
	inet_ntop(AF_INET, &addr->sin_addr.s_addr, ip_addr, INET_ADDRSTRLEN);
}

unsigned short sockaddr_in_port(struct sockaddr_in *addr)
{
	return ntohs(addr->sin_port);
}
