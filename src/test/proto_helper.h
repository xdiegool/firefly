#ifndef PROTO_HELPER_H
#define PROTO_HELPER_H

#include "protocol/firefly_protocol_private.h"
#include <protocol/firefly_protocol.h>

#define ENCODER_WRITE_BUF_SIZE (128)
#define DATA_SAMPLE_DATA_SIZE (128)

#define IMPORTANT_ID (1)

struct tmp_data {
	unsigned char *data;
	size_t size;
};

struct firefly_connection *setup_test_conn_new(
	       struct firefly_connection_actions *conn_actions,
	       struct firefly_event_queue *eq);

void chan_opened_mock(struct firefly_channel *chan);

void transport_ack_test(unsigned char id, struct firefly_connection *conn);

void transport_write_test_decoder(unsigned char *data, size_t size,
					  struct firefly_connection *conn, bool important,
					   unsigned char *id);

bool chan_open_recv_accept_open(struct firefly_channel *chan);

void chan_open_recv_write_open(unsigned char *data, size_t size,
							   struct firefly_connection *conn, bool important,
					   unsigned char *id);

bool chan_open_recv_accept_recv(struct firefly_channel *chan);

void chan_open_recv_write_recv(unsigned char *data, size_t size,
					   struct firefly_connection *conn, bool important,
					   unsigned char *id);

void free_tmp_data(struct tmp_data *td);

int init_labcomm_test_enc_dec();
int init_labcomm_test_enc_dec_custom(struct labcomm_reader *test_r,
		struct labcomm_writer *test_w);
int clean_labcomm_test_enc_dec();

void test_handle_channel_close(firefly_protocol_channel_close *d, void *ctx);
void test_handle_channel_ack(firefly_protocol_channel_ack *d, void *ctx);
void test_handle_channel_response(firefly_protocol_channel_response *d, void *ctx);
void test_handle_channel_request(firefly_protocol_channel_request *d, void *ctx);
void test_handle_data_sample(firefly_protocol_data_sample *d, void *ctx);

#endif
