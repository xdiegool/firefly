#define _POSIX_C_SOURCE (200112L)
#include <pthread.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <CUnit/Basic.h>
#include <CUnit/Console.h>
#include <labcomm.h>
#include <labcomm_private.h>
#include <gen/firefly_protocol.h>
#include <gen/test.h>
#include <utils/firefly_event_queue.h>
#include <transport/firefly_transport_udp_posix.h>
#include <protocol/firefly_protocol.h>
#include <utils/cppmacros.h>

#include "test/test_labcomm_utils.h"
#include "test/event_helper.h"

#define MOCK_UDP_PORT (51337)
#define FIREFLY_UDP_PORT (51338)
#define IP_ADDR ("127.0.0.1")

#define MOCK_CHANNEL_ID (2)

#define MOCK_FIREFLY_BUFFER_SIZE (128)

#define NBR_TIME_SAMPLES (100)
#define RESEND_TIMEOUT (500)
#define TIME_MAX_DIFF (30)

#define MOCK_RECEIVED (-1)
#define MOCK_SENT (1)

enum packet_type {
	CHANNEL_REQUEST_TYPE,
	CHANNEL_RESPONSE_TYPE,
	CHANNEL_ACK_TYPE,
	CHANNEL_CLOSE_TYPE,
	DATA_SAMPLE_TYPE,
	ACK_TYPE
};

struct mock_data_collector {
	void *packet;
	enum packet_type type;
	char direction;
	struct timespec timestamp;
	struct mock_data_collector *next;
	struct mock_data_collector *prev;
};

struct mock_firefly_writer_context {
	int socket;
	struct sockaddr_in *local_addr;
	struct sockaddr_in *remote_addr;
	bool signature;
};

static int mock_firefly_reader_alloc(struct labcomm_reader *r, void *context,
		    struct labcomm_decoder *decoder,
		    char *version)
{
	UNUSED_VAR(context);
	UNUSED_VAR(decoder);
	UNUSED_VAR(version);
	int result = 0;
	r->data = malloc(MOCK_FIREFLY_BUFFER_SIZE);
	if (r->data == NULL) {
		result = -ENOMEM;
	} else {
		r->data_size = MOCK_FIREFLY_BUFFER_SIZE;
		r->pos = 0;
		r->count = 0;
	}
	return result;
}

static int mock_firefly_reader_free(struct labcomm_reader *r, void *context)
{
	UNUSED_VAR(context);
	free(r->data);
	free(r);
	return 0;
}

static int mock_firefly_reader_fill(struct labcomm_reader *r, void *context)
{
	int socket = *((int *) context);
	int result = r->count - r->pos;
	if (result <= 0) {
		fd_set fs;
		FD_ZERO(&fs);
		FD_SET(socket, &fs);
		result = select(socket + 1, &fs, NULL, NULL, NULL);
		if (result == -1) {
			return result;
		}
		size_t pkg_len = 0;
		result = ioctl(socket, FIONREAD, &pkg_len);
		if (result) {
			return result;
		}
		r->pos = 0;
		result = recv(socket, r->data, pkg_len, 0); 
		if (result == -1) {
			return result;
		}
		r->count = pkg_len;
	}
	return result < 0 ? -ENOMEM : result;
}

static int mock_firefly_reader_start(struct labcomm_reader *r, void *context)
{
	return mock_firefly_reader_fill(r, context);
}

static int mock_firefly_reader_end(struct labcomm_reader *r, void *context)
{
	UNUSED_VAR(r);
	UNUSED_VAR(context);
	return 0;
}

static int mock_firefly_reader_ioctl(struct labcomm_reader *r, void *context, 
								int action,
								struct labcomm_signature *signature,
								va_list arg)
{
	UNUSED_VAR(r);
	UNUSED_VAR(context);
	UNUSED_VAR(action);
	UNUSED_VAR(signature);
	UNUSED_VAR(arg);

	int result = -ENOTSUP;
	return result;
}

static const struct labcomm_reader_action mock_firefly_reader_action = {
	.alloc = mock_firefly_reader_alloc,
	.free = mock_firefly_reader_free,
	.start = mock_firefly_reader_start,
	.fill = mock_firefly_reader_fill,
	.end = mock_firefly_reader_end,
	.ioctl = mock_firefly_reader_ioctl
};

struct labcomm_reader *mock_firefly_labcomm_reader_new(int *fd)
{
	struct labcomm_reader *result;

	result = malloc(sizeof(*result));
	if (result != NULL) {
		result->context = fd;
		result->action = &mock_firefly_reader_action;
	}
	return result;
}

void *mock_reader_run(void *args)
{
	struct labcomm_decoder *dec = (struct labcomm_decoder *) args;
	labcomm_decoder_run(dec);
	return NULL;
}

static int mock_firefly_writer_alloc(struct labcomm_writer *w, void *context,
		struct labcomm_encoder *encoder, char *labcomm_version)
{
	UNUSED_VAR(context);
	UNUSED_VAR(encoder);
	UNUSED_VAR(labcomm_version);
	w->error = 0;
	w->data = malloc(MOCK_FIREFLY_BUFFER_SIZE);
	if (w->data == NULL) {
		w->data_size = 0;
		w->count = 0;
		w->error = -ENOMEM;
	} else {
		w->data_size = MOCK_FIREFLY_BUFFER_SIZE;
		w->count = MOCK_FIREFLY_BUFFER_SIZE;
		w->pos = 0;
	}

	return w->error;
}

static int mock_firefly_writer_free(struct labcomm_writer *w, void *context)
{
	free(w->data);
	free(context);
	free(w);
	return 0;
}

static int mock_firefly_writer_flush(struct labcomm_writer *w, void *context)
{
	UNUSED_VAR(context);
	int result = w->count - w->pos;
	return result < 0 ? -ENOMEM : result;
}

static int mock_firefly_writer_start(struct labcomm_writer *w, void *context,
		struct labcomm_encoder *encoder, int index,
		struct labcomm_signature *signature, void *value)
{
	UNUSED_VAR(w);
	UNUSED_VAR(context);
	UNUSED_VAR(encoder);
	UNUSED_VAR(index);
	UNUSED_VAR(signature);
	UNUSED_VAR(value);
	struct mock_firefly_writer_context *ctx = context;
	ctx->signature = (value == NULL);
	w->pos = 0;

	return 0;
}

static int mock_firefly_writer_end(struct labcomm_writer *w, void *context)
{

	struct mock_firefly_writer_context *ctx = context;
	int result = sendto(ctx->socket, w->data, w->pos, 0,
			(struct sockaddr *) (ctx->signature ? ctx->local_addr : ctx->remote_addr), sizeof(struct sockaddr_in));
	w->pos = 0;
	return result < 0 ? result : 0;
}

static int mock_firefly_writer_ioctl(struct labcomm_writer *w, void *context,
		int action, struct labcomm_signature *signature, va_list arg)
{
	UNUSED_VAR(w);
	UNUSED_VAR(context);
	UNUSED_VAR(action);
	UNUSED_VAR(signature);
	UNUSED_VAR(arg);
	int result = -ENOTSUP;
	return result;
}

static const struct labcomm_writer_action mock_firefly_writer_action = {
	.alloc = mock_firefly_writer_alloc,
	.free = mock_firefly_writer_free,
	.start = mock_firefly_writer_start,
	.end = mock_firefly_writer_end,
	.flush = mock_firefly_writer_flush,
	.ioctl = mock_firefly_writer_ioctl
};

struct labcomm_writer *mock_firefly_labcomm_writer_new(
		int socket, struct sockaddr_in *loc, struct sockaddr_in *rem)
{
	struct labcomm_writer *result;
	struct mock_firefly_writer_context *context;

	result = malloc(sizeof(*result));
	context = malloc(sizeof(*context));
	if (result != NULL && context != NULL) {
		context->socket = socket;
		context->local_addr = loc;
		context->remote_addr = rem;
		context->signature = false;
		result->context = context;
		result->action = &mock_firefly_writer_action;
	} else {
		free(result);
		result = NULL;
		free(context);
	}

	return result;
}

static inline long timespec_diff_ms(struct timespec *from, struct timespec *to)
{
	return (to->tv_sec - from->tv_sec)*1000 +
		(to->tv_nsec - from->tv_nsec)/1000000;
}

static inline long timespec_diff_ns(struct timespec *from, struct timespec *to)
{
	return (to->tv_sec - from->tv_sec)*1000000000 +
		(to->tv_nsec - from->tv_nsec);
}

static inline void timespec_print(struct timespec *t)
{
	printf("%ld.%09ld", t->tv_sec, t->tv_nsec);
}

static pthread_mutex_t mock_data_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t mock_data_signal = PTHREAD_COND_INITIALIZER;
static struct mock_data_collector *data_collection = NULL;
static struct mock_data_collector *last_packet = NULL;
static int nbr_packets = 0;

static bool firefly_channel_open_status = false;

static void mock_data_collector_packet_free(void *packet, enum packet_type type)
{
	if (type == DATA_SAMPLE_TYPE) {
		firefly_protocol_data_sample *ds =
			(firefly_protocol_data_sample *) packet;
		free(ds->app_enc_data.a);
	}
	free(packet);
}

static void mock_data_collector_free(struct mock_data_collector *p)
{
	if (p->next != NULL) {
		mock_data_collector_free(p->next);
	}
	mock_data_collector_packet_free(p->packet, p->type);
	free(p);
}

static void add_packet(struct mock_data_collector *p)
{
	if (data_collection == NULL) {
		data_collection = p;
		p->prev = NULL;
	} else {
		last_packet->next = p;
		p->prev = last_packet;
	}
	last_packet = p;
}
static void sent_packet(void *packet, enum packet_type type)
{
	++nbr_packets;
	struct mock_data_collector *new_packet;
	new_packet = malloc(sizeof(struct mock_data_collector));
	new_packet->packet = packet;
	new_packet->type = type;
	new_packet->direction = MOCK_SENT;
	new_packet->next = NULL;
	clock_gettime(CLOCK_REALTIME, &new_packet->timestamp);
	add_packet(new_packet);
}

static void received_packet(void *packet, enum packet_type type)
{
	++nbr_packets;
	struct mock_data_collector *new_packet;
	new_packet = malloc(sizeof(struct mock_data_collector));
	new_packet->packet = packet;
	new_packet->type = type;
	new_packet->direction = MOCK_RECEIVED;
	new_packet->next = NULL;
	clock_gettime(CLOCK_REALTIME, &new_packet->timestamp);
	add_packet(new_packet);
}

static struct mock_data_collector *next_packet_wait(
		struct mock_data_collector *current)
{
	struct mock_data_collector *res;
	pthread_mutex_lock(&mock_data_lock);
	if (current == NULL) {
		while (data_collection == NULL) {
			pthread_cond_wait(&mock_data_signal, &mock_data_lock);
		}
		res = data_collection;
	} else {
		while (current->next == NULL) {
			pthread_cond_wait(&mock_data_signal, &mock_data_lock);
		}
		res = current->next;
	}
	pthread_mutex_unlock(&mock_data_lock);
	return res;
}
static int mock_firefly_app_reader_alloc(struct labcomm_reader *r, void *context,
		    struct labcomm_decoder *decoder,
		    char *version)
{
	UNUSED_VAR(context);
	UNUSED_VAR(decoder);
	UNUSED_VAR(version);
	int result = 0;
	r->data = NULL;
	r->data_size = 0;
	r->pos = 0;
	r->count = 0;
	return result;
}

static int mock_firefly_app_reader_free(struct labcomm_reader *r, void *context)
{
	UNUSED_VAR(context);
	free(r);
	return 0;
}

static int mock_firefly_app_reader_fill(struct labcomm_reader *r, void *context)
{
	int result = 0;
	static struct mock_data_collector *last_decoded_packet;
	struct mock_data_collector *packet =
		*((struct mock_data_collector **) context);
	if (packet->type == DATA_SAMPLE_TYPE && packet != last_decoded_packet) {
		firefly_protocol_data_sample *d =
			((firefly_protocol_data_sample *) packet->packet);
		r->data = d->app_enc_data.a;
		r->count = d->app_enc_data.n_0;
		r->data_size = d->app_enc_data.n_0;
		r->pos = 0;
		result = r->count;
		last_decoded_packet = packet;
	}
	return result < 0 ? -ENOMEM : result;
}

static int mock_firefly_app_reader_start(struct labcomm_reader *r, void *context)
{
	return mock_firefly_app_reader_fill(r, context);
}

static const struct labcomm_reader_action mock_firefly_app_reader_action = {
	.alloc = mock_firefly_app_reader_alloc,
	.free = mock_firefly_app_reader_free,
	.start = mock_firefly_app_reader_start,
	.fill = mock_firefly_app_reader_fill,
	.end = mock_firefly_reader_end,
	.ioctl = mock_firefly_reader_ioctl
};

struct labcomm_reader *mock_firefly_labcomm_app_reader_new(
		struct mock_data_collector **d)
{
	struct labcomm_reader *result;

	result = malloc(sizeof(*result));
	if (result != NULL) {
		result->context = d;
		result->action = &mock_firefly_app_reader_action;
	}
	return result;
}

void test_handle_data_sample(firefly_protocol_data_sample *d, void *ctx)
{
	static int last_seqno;
	static int resent;
	if (d->important && d->seqno > last_seqno) {
		last_seqno = d->seqno;
		resent = 0;
	} else if (d->important && last_seqno == d->seqno) {
		++resent;
	}
	firefly_protocol_data_sample *d_cpy =
		malloc(sizeof(firefly_protocol_data_sample));
	memcpy(d_cpy, d, sizeof(firefly_protocol_data_sample));
	d_cpy->app_enc_data.a = malloc(d->app_enc_data.n_0);
	d_cpy->app_enc_data.n_0 = d->app_enc_data.n_0;
	memcpy(d_cpy->app_enc_data.a, d->app_enc_data.a, d->app_enc_data.n_0);
	pthread_mutex_lock(&mock_data_lock);
	received_packet(d_cpy, DATA_SAMPLE_TYPE);
	pthread_cond_signal(&mock_data_signal);
	pthread_mutex_unlock(&mock_data_lock);
	if (d->important && resent == 1 && d->seqno < 2) {
		struct labcomm_encoder *enc = (struct labcomm_encoder *) ctx;
		firefly_protocol_ack *ack = malloc(sizeof(*ack));;
		ack->src_chan_id = MOCK_CHANNEL_ID;
		ack->dest_chan_id = d->src_chan_id;
		ack->seqno = d->seqno;
		pthread_mutex_lock(&mock_data_lock);
		labcomm_encode_firefly_protocol_ack(enc, ack);
		sent_packet(ack, ACK_TYPE);
		pthread_cond_signal(&mock_data_signal);
		pthread_mutex_unlock(&mock_data_lock);
	}
}

void test_handle_channel_request(firefly_protocol_channel_request *d, void *ctx)
{
	static int nbr_reqs;
	nbr_reqs++;
	firefly_protocol_channel_request *d_cpy = malloc(sizeof(*d));
	memcpy(d_cpy, d, sizeof(*d));
	pthread_mutex_lock(&mock_data_lock);
	received_packet(d_cpy, CHANNEL_REQUEST_TYPE);
	pthread_cond_signal(&mock_data_signal);
	pthread_mutex_unlock(&mock_data_lock);
	if (nbr_reqs == 2) {
		struct labcomm_encoder *enc = (struct labcomm_encoder *) ctx;
		firefly_protocol_channel_response *resp = malloc(sizeof(*resp));;
		resp->source_chan_id = MOCK_CHANNEL_ID;
		resp->dest_chan_id = d->source_chan_id;
		resp->ack = true;
		pthread_mutex_lock(&mock_data_lock);
		labcomm_encode_firefly_protocol_channel_response(enc, resp);
		sent_packet(resp, CHANNEL_RESPONSE_TYPE);
		pthread_cond_signal(&mock_data_signal);
		pthread_mutex_unlock(&mock_data_lock);
	} else if (nbr_reqs > 2) {
		CU_FAIL("Got more than 2 channel requests");
	}
}

void test_handle_channel_response(firefly_protocol_channel_response *d, void *ctx)
{
	UNUSED_VAR(d);
	UNUSED_VAR(ctx);
	firefly_protocol_channel_response *d_cpy = malloc(sizeof(*d));
	memcpy(d_cpy, d, sizeof(*d));
	pthread_mutex_lock(&mock_data_lock);
	received_packet(d_cpy, CHANNEL_RESPONSE_TYPE);
	pthread_cond_signal(&mock_data_signal);
	pthread_mutex_unlock(&mock_data_lock);
}

void test_handle_channel_ack(firefly_protocol_channel_ack *d, void *ctx)
{
	UNUSED_VAR(d);
	UNUSED_VAR(ctx);
	firefly_protocol_channel_ack *d_cpy = malloc(sizeof(*d));
	memcpy(d_cpy, d, sizeof(*d));
	pthread_mutex_lock(&mock_data_lock);
	received_packet(d_cpy, CHANNEL_ACK_TYPE);
	pthread_cond_signal(&mock_data_signal);
	pthread_mutex_unlock(&mock_data_lock);
}

void test_handle_channel_close(firefly_protocol_channel_close *d, void *ctx)
{
	UNUSED_VAR(d);
	UNUSED_VAR(ctx);
	firefly_protocol_channel_close *d_cpy = malloc(sizeof(*d));
	memcpy(d_cpy, d, sizeof(*d));
	pthread_mutex_lock(&mock_data_lock);
	received_packet(d_cpy, CHANNEL_CLOSE_TYPE);
	pthread_cond_signal(&mock_data_signal);
	pthread_mutex_unlock(&mock_data_lock);
}

void test_handle_ack(firefly_protocol_ack *d, void *ctx)
{
	UNUSED_VAR(d);
	UNUSED_VAR(ctx);
	firefly_protocol_ack *d_cpy = malloc(sizeof(*d));
	memcpy(d_cpy, d, sizeof(*d));
	pthread_mutex_lock(&mock_data_lock);
	received_packet(d_cpy, ACK_TYPE);
	pthread_cond_signal(&mock_data_signal);
	pthread_mutex_unlock(&mock_data_lock);
}

// FIREFLY stuff
struct event_queue_signals {
	pthread_mutex_t eq_lock;
	pthread_cond_t eq_cond;
	bool event_exec_finish;
};

int event_add_mutex(struct firefly_event_queue *eq, unsigned char prio,
		firefly_event_execute_f execute, void *context)
{
	int res = 0;
	struct event_queue_signals *eq_s =
		(struct event_queue_signals *)firefly_event_queue_get_context(eq);

	res = pthread_mutex_lock(&eq_s->eq_lock);
	if (res) {
		return res;
	}
	res = firefly_event_add(eq, prio, execute, context);
	if (!res) {
		pthread_cond_signal(&eq_s->eq_cond);
	}
	pthread_mutex_unlock(&eq_s->eq_lock);
	return res;
}

void *event_thread_main(void *args)
{
	struct firefly_event_queue *eq =
		(struct firefly_event_queue *) args;
	struct event_queue_signals *eq_s =
		(struct event_queue_signals *)
		firefly_event_queue_get_context(eq);
	struct firefly_event *ev = NULL;
	pthread_mutex_lock(&eq_s->eq_lock);
	bool finish = eq_s->event_exec_finish;
	int event_left = firefly_event_queue_length(eq);
	pthread_mutex_unlock(&eq_s->eq_lock);

	while (!finish || event_left > 0) {
		pthread_mutex_lock(&eq_s->eq_lock);
		event_left = firefly_event_queue_length(eq);
		finish = eq_s->event_exec_finish;
		while (event_left < 1 && !finish) {
			pthread_cond_wait(&eq_s->eq_cond, &eq_s->eq_lock);
			finish = eq_s->event_exec_finish;
			event_left = firefly_event_queue_length(eq);
		}
		ev = firefly_event_pop(eq);
		pthread_mutex_unlock(&eq_s->eq_lock);
		if (ev != NULL) {
			firefly_event_execute(ev);
			firefly_event_return(eq, &ev);
		}
	}
	return NULL;
}

void firefly_handle_test_var(test_test_var *d, void *ctx)
{
	UNUSED_VAR(d);
	UNUSED_VAR(ctx);
}

void firefly_handle_test_var_2(test_test_var_2 *d, void *ctx)
{
	UNUSED_VAR(d);
	UNUSED_VAR(ctx);
}

void firefly_handle_time_sample(test_time_sample *d, void *ctx)
{
	UNUSED_VAR(d);
	UNUSED_VAR(ctx);
}

static long acc_delay = 0;
void mock_handle_time_sample(test_time_sample *d, void *ctx)
{
	struct mock_data_collector *packet = *((struct mock_data_collector **) ctx);
	struct timespec sent;
	sent.tv_sec = d->sec;
	sent.tv_nsec = d->nsec;
	long diff = timespec_diff_ns(&sent, &packet->timestamp);
	acc_delay += diff;
}

struct firefly_connection *received_connection(
		struct firefly_transport_llp *llp,
		const char *ip_addr, unsigned short port)
{
	UNUSED_VAR(llp);
	UNUSED_VAR(ip_addr);
	UNUSED_VAR(port);
	return NULL;
}

static struct firefly_channel *firefly_chan = NULL;
void firefly_channel_opened(struct firefly_channel *chan)
{
	struct labcomm_encoder *out = firefly_protocol_get_output_stream(chan);
	struct labcomm_decoder *in = firefly_protocol_get_input_stream(chan);
	labcomm_decoder_register_test_time_sample(in, firefly_handle_time_sample, NULL);
	labcomm_encoder_register_test_time_sample(out);
	pthread_mutex_lock(&mock_data_lock);
	firefly_channel_open_status = true;
	firefly_chan = chan;
	pthread_cond_signal(&mock_data_signal);
	pthread_mutex_unlock(&mock_data_lock);
}

static struct timespec closed_at;
void firefly_channel_closed(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	pthread_mutex_lock(&mock_data_lock);
	firefly_channel_open_status = false;
	clock_gettime(CLOCK_REALTIME, &closed_at);
	pthread_cond_signal(&mock_data_signal);
	pthread_mutex_unlock(&mock_data_lock);
}

bool firefly_channel_received(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	printf("channel received\n");
	return false;
}

void test_something()
{
	printf("\n");
	int res;
	pthread_t event_thread;
	pthread_t mock_reader_thread;
	pthread_t resend_thread;
	pthread_t reader_thread;
	int mock_socket = 0;
	struct sockaddr_in local_addr;
	memset(&local_addr, 0, sizeof(local_addr));
	struct sockaddr_in remote_addr;
	memset(&remote_addr, 0, sizeof(remote_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(MOCK_UDP_PORT);
	inet_pton(AF_INET, IP_ADDR, &local_addr.sin_addr);
	mock_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	bind(mock_socket, (struct sockaddr *) &local_addr,
			sizeof(struct sockaddr_in));

	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(FIREFLY_UDP_PORT);
	inet_pton(AF_INET, IP_ADDR, &remote_addr.sin_addr);

	struct mock_data_collector *prev_packet = NULL;
	struct mock_data_collector *current_packet = NULL;

	struct labcomm_encoder *mock_out;
	struct labcomm_decoder *mock_in;
	struct labcomm_decoder *mock_data_in;
	mock_out = labcomm_encoder_new(mock_firefly_labcomm_writer_new(mock_socket,
				&local_addr, &remote_addr), NULL);
	mock_in = labcomm_decoder_new(
			mock_firefly_labcomm_reader_new(&mock_socket), NULL);
	mock_data_in = labcomm_decoder_new(
			mock_firefly_labcomm_app_reader_new(&current_packet), NULL);
	labcomm_register_error_handler_encoder(mock_out, handle_labcomm_error);
	labcomm_register_error_handler_decoder(mock_in, handle_labcomm_error);
	labcomm_register_error_handler_decoder(mock_data_in, handle_labcomm_error);

	labcomm_decoder_register_firefly_protocol_data_sample(mock_in,
						test_handle_data_sample, mock_out);
	labcomm_decoder_register_firefly_protocol_channel_request(mock_in,
						test_handle_channel_request, mock_out);
	labcomm_decoder_register_firefly_protocol_channel_response(mock_in,
						test_handle_channel_response, mock_out);
	labcomm_decoder_register_firefly_protocol_channel_ack(mock_in,
						test_handle_channel_ack, mock_out);
	labcomm_decoder_register_firefly_protocol_channel_close(mock_in,
						test_handle_channel_close, mock_out);
	labcomm_decoder_register_firefly_protocol_ack(mock_in,
						test_handle_ack, mock_out);

	labcomm_encoder_register_firefly_protocol_data_sample(mock_out);
	labcomm_encoder_register_firefly_protocol_channel_request(mock_out);
	labcomm_encoder_register_firefly_protocol_channel_response(mock_out);
	labcomm_encoder_register_firefly_protocol_channel_ack(mock_out);
	labcomm_encoder_register_firefly_protocol_channel_close(mock_out);
	labcomm_encoder_register_firefly_protocol_ack(mock_out);

	labcomm_decoder_register_test_time_sample(mock_data_in,
			mock_handle_time_sample, &current_packet);

	res = pthread_create(&mock_reader_thread, NULL, mock_reader_run, mock_in);
	if (res) {
		fprintf(stderr, "ERROR: starting mock reader thread.\n");
	}

	struct event_queue_signals eq_s;
	res = pthread_mutex_init(&eq_s.eq_lock, NULL);
	if (res) {
		fprintf(stderr, "ERROR: init mutex.\n");
	}
	res = pthread_cond_init(&eq_s.eq_cond, NULL);
	if (res) {
		fprintf(stderr, "ERROR: init cond variable.\n");
	}
	eq_s.event_exec_finish = false;
	struct firefly_event_queue *events =
		firefly_event_queue_new(event_add_mutex, 10, &eq_s);
	res = pthread_create(&event_thread, NULL, event_thread_main, events);
	if (res) {
		fprintf(stderr, "ERROR: starting event thread.\n");
	}
	struct firefly_transport_llp *llp =
		firefly_transport_llp_udp_posix_new(FIREFLY_UDP_PORT,
				received_connection, events);
	res = firefly_transport_udp_posix_run(llp, &reader_thread, &resend_thread);
	if (res) {
		fprintf(stderr, "ERROR: starting reader/resend thread.\n");
	}

	struct firefly_connection_actions conn_actions = {
		.channel_opened		= firefly_channel_opened,
		.channel_closed		= firefly_channel_closed,
		.channel_recv		= firefly_channel_received,
		// New -v
		.channel_rejected	= NULL,
		.channel_restrict	= NULL,
		.channel_restrict_info	= NULL
	};

	struct firefly_connection *conn =
		firefly_transport_connection_udp_posix_open(llp,
				IP_ADDR, MOCK_UDP_PORT, RESEND_TIMEOUT,
				&conn_actions);
	firefly_channel_open(conn);

	// Receive ingored request
	current_packet = next_packet_wait(prev_packet);
	prev_packet = current_packet;

	// Receive resent request
	current_packet = next_packet_wait(prev_packet);
	long time_diff = timespec_diff_ms(&prev_packet->timestamp,
			&current_packet->timestamp);
	CU_ASSERT_FALSE(time_diff < RESEND_TIMEOUT - TIME_MAX_DIFF ||
			time_diff > RESEND_TIMEOUT + TIME_MAX_DIFF);
	prev_packet = current_packet;

	// Sent response
	current_packet = next_packet_wait(prev_packet);
	CU_ASSERT_EQUAL(current_packet->type, CHANNEL_RESPONSE_TYPE);
	prev_packet = current_packet;

	// Receive channel ack
	current_packet = next_packet_wait(prev_packet);
	CU_ASSERT_EQUAL(current_packet->type, CHANNEL_ACK_TYPE);
	prev_packet = current_packet;

	// receive data important, ignore
	current_packet = next_packet_wait(prev_packet);
	CU_ASSERT_EQUAL(current_packet->type, DATA_SAMPLE_TYPE);
	prev_packet = current_packet;

	// receive data important
	current_packet = next_packet_wait(prev_packet);
	CU_ASSERT_EQUAL(current_packet->type, DATA_SAMPLE_TYPE);
	// let decoder decode signature for later
	pthread_mutex_lock(&mock_data_lock);
	labcomm_decoder_decode_one(mock_data_in);
	pthread_mutex_unlock(&mock_data_lock);
	prev_packet = current_packet;

	// Sent ack packet
	current_packet = next_packet_wait(prev_packet);
	CU_ASSERT_EQUAL(current_packet->type, ACK_TYPE);
	prev_packet = current_packet;

	struct mock_data_collector *tmp_packet = current_packet;
	// Receive data sample
	test_time_sample t;
	struct timespec now;
	for (int i = 0; i < NBR_TIME_SAMPLES; i++) {
		clock_gettime(CLOCK_REALTIME, &now);
		t.sec = now.tv_sec;
		t.nsec = now.tv_nsec;
		labcomm_encode_test_time_sample(
				firefly_protocol_get_output_stream(firefly_chan), &t);
		/*printf("send %d...", i);*/
		tmp_packet = next_packet_wait(tmp_packet);
		CU_ASSERT_EQUAL(tmp_packet->type, DATA_SAMPLE_TYPE);
		/*printf("done\n");*/
	}

	for (int i = 0; i < NBR_TIME_SAMPLES; i++) {
		/*printf("wait %d...", i);*/
		current_packet = next_packet_wait(prev_packet);
		/*printf("done\n");*/
		CU_ASSERT_EQUAL(current_packet->type, DATA_SAMPLE_TYPE);
		pthread_mutex_lock(&mock_data_lock);
		labcomm_decoder_decode_one(mock_data_in);
		pthread_mutex_unlock(&mock_data_lock);
		prev_packet = current_packet;
	}
	printf("Average packet delay (ns): %f\n", ((float) acc_delay / NBR_TIME_SAMPLES));

	labcomm_decoder_register_test_test_var_2(
			firefly_protocol_get_input_stream(firefly_chan),
			firefly_handle_test_var_2, NULL);
	labcomm_encoder_register_test_test_var_2(
			firefly_protocol_get_output_stream(firefly_chan));

	printf("Testing resend timeout error, should take 3 sec\n");
	// receive first data important, ignore
	current_packet = next_packet_wait(prev_packet);
	CU_ASSERT_EQUAL(current_packet->type, DATA_SAMPLE_TYPE);
	prev_packet = current_packet;
	// receive 5 data important, ignore
	for (int i = 0; i < 5; i++) {
		current_packet = next_packet_wait(prev_packet);
		CU_ASSERT_EQUAL(current_packet->type, DATA_SAMPLE_TYPE);
		time_diff = timespec_diff_ms(&prev_packet->timestamp,
				&current_packet->timestamp);
		CU_ASSERT_FALSE(time_diff < RESEND_TIMEOUT - TIME_MAX_DIFF ||
				time_diff > RESEND_TIMEOUT + TIME_MAX_DIFF);
		prev_packet = current_packet;
	}

	// wait for channel to be closed
	pthread_mutex_lock(&mock_data_lock);
	while (firefly_channel_open_status) {
		pthread_cond_wait(&mock_data_signal, &mock_data_lock);
	}
	time_diff = timespec_diff_ms(&prev_packet->timestamp,
			&closed_at);
	CU_ASSERT_FALSE(time_diff < RESEND_TIMEOUT - TIME_MAX_DIFF ||
			time_diff > RESEND_TIMEOUT + TIME_MAX_DIFF);
	pthread_mutex_unlock(&mock_data_lock);

	firefly_transport_udp_posix_stop(llp, &reader_thread, &resend_thread);
	firefly_transport_llp_udp_posix_free(llp);
	
	// Stop firefly
	pthread_mutex_lock(&eq_s.eq_lock);
	eq_s.event_exec_finish = true;
	pthread_cond_signal(&eq_s.eq_cond);
	pthread_mutex_unlock(&eq_s.eq_lock);

	pthread_join(event_thread, NULL);
	firefly_event_queue_free(&events);

	pthread_cancel(mock_reader_thread);
	pthread_join(mock_reader_thread, NULL);

	//clean up
	close(mock_socket);
	labcomm_encoder_free(mock_out);
	labcomm_decoder_free(mock_in);
	labcomm_decoder_free(mock_data_in);
	mock_data_collector_free(data_collection);
}

int init_suit_system()
{
	return 0;
}

int clean_suit_system()
{
	return 0;
}

int main()
{
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}
	CU_pSuite system_suite = NULL;
	system_suite = CU_add_suite("system_test",
			init_suit_system, clean_suit_system);
	if (system_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	if (
			(CU_add_test(system_suite, "test_something",
					test_something) == NULL)
	   ) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	// Set verbosity.
	CU_basic_set_mode(CU_BRM_VERBOSE);
	/*CU_console_run_tests();*/

	// Run all test suites.
	CU_basic_run_tests();
	int res = CU_get_number_of_tests_failed();
	// Clean up.
	CU_cleanup_registry();

	if (res != 0) {
		return 1;
	}
}
