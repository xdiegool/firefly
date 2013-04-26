#include "test/proto_helper.h"

#include <stdlib.h>
#include <labcomm.h>
#include <test/labcomm_mem_writer.h>
#include <test/labcomm_mem_reader.h>
#include <labcomm_fd_reader_writer.h>
#include "CUnit/Basic.h"
#include <stdbool.h>

#include <gen/firefly_protocol.h>
#include <gen/test.h>
#include <utils/firefly_errors.h>
#include <utils/firefly_event_queue.h>

#include "test/test_labcomm_utils.h"
#include "utils/cppmacros.h"

labcomm_mem_reader_context_t *test_dec_ctx = NULL;
struct labcomm_decoder *test_dec = NULL;
struct labcomm_encoder *test_enc;
labcomm_mem_writer_context_t *test_enc_ctx;

unsigned char data_sample_data[DATA_SAMPLE_DATA_SIZE];
firefly_protocol_data_sample data_sample;
firefly_protocol_channel_request channel_request;
firefly_protocol_channel_response channel_response;
firefly_protocol_channel_ack channel_ack;
firefly_protocol_channel_close channel_close;
firefly_protocol_ack ack;

bool received_data_sample = false;
bool received_channel_request = false;
bool received_channel_response = false;
bool received_channel_ack = false;
bool received_channel_close = false;
bool received_ack = false;

struct tmp_data conn_open_write;
struct tmp_data conn_recv_write;
bool conn_recv_accept_called = false;

struct firefly_connection *setup_test_conn_new(firefly_channel_is_open_f ch_op,
		firefly_channel_closed_f ch_cl, firefly_channel_accept_f ch_acc,
		struct firefly_event_queue *eq)
{
	struct firefly_connection *conn =
		firefly_connection_new(ch_op, ch_cl, ch_acc,
				transport_write_test_decoder, NULL, eq, NULL, NULL);
	if (conn == NULL) {
		CU_FAIL("Could not create connection.\n");
	}
	// Set some stuff that firefly_init_conn doesn't do
	labcomm_register_error_handler_encoder(conn->transport_encoder,
			handle_labcomm_error);
	labcomm_register_error_handler_decoder(conn->transport_decoder,
			handle_labcomm_error);
	return conn;
}

bool chan_opened_called = false;
void chan_opened_mock(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	chan_opened_called = true;
}

void transport_write_test_decoder(unsigned char *data, size_t size,
					  struct firefly_connection *conn, bool important,
					   unsigned char *id)
{
	UNUSED_VAR(conn);
	UNUSED_VAR(important);
	UNUSED_VAR(id);
	test_dec_ctx->enc_data = data;
	test_dec_ctx->size = size;
	labcomm_decoder_decode_one(test_dec);
	test_dec_ctx->enc_data = NULL;
	test_dec_ctx->size = 0;
}

bool chan_open_recv_accept_open(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	CU_FAIL("Connection open received channel.");
	return false;
}

void chan_open_recv_write_open(unsigned char *data, size_t size,
					   struct firefly_connection *conn, bool important,
					   unsigned char *id)
{
	UNUSED_VAR(conn);
	UNUSED_VAR(important);
	UNUSED_VAR(id);
	conn_open_write.data = malloc(size);
	memcpy(conn_open_write.data, data, size);
	conn_open_write.size = size;
}

bool chan_open_recv_accept_recv(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	return conn_recv_accept_called = true;
}

void chan_open_recv_write_recv(unsigned char *data, size_t size,
					   struct firefly_connection *conn, bool important,
					   unsigned char *id)
{
	UNUSED_VAR(conn);
	UNUSED_VAR(important);
	UNUSED_VAR(id);
	conn_recv_write.data = malloc(size);
	memcpy(conn_recv_write.data, data, size);
	conn_recv_write.size = size;
}

void free_tmp_data(struct tmp_data *td)
{
	free(td->data);
	td->data = NULL;
	td->size = 0;
}

void test_handle_data_sample(firefly_protocol_data_sample *d, void *ctx)
{
	UNUSED_VAR(ctx);
	memcpy(&data_sample, d, sizeof(firefly_protocol_data_sample));
	if (d->app_enc_data.n_0 > DATA_SAMPLE_DATA_SIZE) {
		CU_FAIL("Received too large data block, modify test accordingly");
	}
	data_sample.app_enc_data.a = data_sample_data;
	data_sample.app_enc_data.n_0 = d->app_enc_data.n_0;

	memcpy(data_sample.app_enc_data.a, d->app_enc_data.a, d->app_enc_data.n_0);
	received_data_sample = true;
}
void test_handle_channel_request(firefly_protocol_channel_request *d, void *ctx)
{
	UNUSED_VAR(ctx);
	memcpy(&channel_request, d, sizeof(firefly_protocol_channel_request));
	received_channel_request = true;
}
void test_handle_channel_response(firefly_protocol_channel_response *d, void *ctx)
{
	UNUSED_VAR(ctx);
	memcpy(&channel_response, d, sizeof(firefly_protocol_channel_response));
	received_channel_response = true;
}
void test_handle_channel_ack(firefly_protocol_channel_ack *d, void *ctx)
{
	UNUSED_VAR(ctx);
	memcpy(&channel_ack, d, sizeof(firefly_protocol_channel_ack));
	received_channel_ack = true;
}
void test_handle_channel_close(firefly_protocol_channel_close *d, void *ctx)
{
	UNUSED_VAR(ctx);
	memcpy(&channel_close, d, sizeof(firefly_protocol_channel_close));
	received_channel_close = true;
}

void test_handle_ack(firefly_protocol_ack *d, void *ctx)
{
	UNUSED_VAR(ctx);
	memcpy(&ack, d, sizeof(firefly_protocol_ack));
	received_ack = true;
}

int init_labcomm_test_enc_dec()
{
	test_enc_ctx = malloc(sizeof(labcomm_mem_writer_context_t));
	if (test_enc_ctx == NULL) {
		return 1;
	}
	test_enc_ctx->buf = malloc(ENCODER_WRITE_BUF_SIZE);
	if (test_enc_ctx->buf == NULL) {
		return 1;
	}
	test_enc_ctx->length = ENCODER_WRITE_BUF_SIZE;
	test_enc_ctx->write_pos = 0;
	test_enc = labcomm_encoder_new(labcomm_mem_writer, test_enc_ctx);
	if (test_enc == NULL) {
		return 1;
	}
	labcomm_register_error_handler_encoder(test_enc, handle_labcomm_error);

	test_dec_ctx = malloc(sizeof(labcomm_mem_reader_context_t));
	if (test_dec_ctx == NULL) {
		CU_FAIL("Test decoder context was null\n");
	}
	test_dec = labcomm_decoder_new(labcomm_mem_reader, test_dec_ctx);
	if (test_dec == NULL) {
		CU_FAIL("Test decoder was null\n");
	}
	labcomm_register_error_handler_decoder(test_dec, handle_labcomm_error);

	labcomm_decoder_register_firefly_protocol_data_sample(test_dec,
						test_handle_data_sample, NULL);
	labcomm_decoder_register_firefly_protocol_channel_request(test_dec,
						test_handle_channel_request, NULL);
	labcomm_decoder_register_firefly_protocol_channel_response(test_dec,
						test_handle_channel_response, NULL);
	labcomm_decoder_register_firefly_protocol_channel_ack(test_dec,
						test_handle_channel_ack, NULL);
	labcomm_decoder_register_firefly_protocol_channel_close(test_dec,
						test_handle_channel_close, NULL);
	labcomm_decoder_register_firefly_protocol_ack(test_dec,
						test_handle_ack, NULL);

	labcomm_encoder_register_firefly_protocol_data_sample(test_enc);
	test_dec_ctx->enc_data = test_enc_ctx->buf;
	test_dec_ctx->size = test_enc_ctx->write_pos;
	labcomm_decoder_decode_one(test_dec);
	test_enc_ctx->write_pos = 0;

	labcomm_encoder_register_firefly_protocol_channel_request(test_enc);
	test_dec_ctx->enc_data = test_enc_ctx->buf;
	test_dec_ctx->size = test_enc_ctx->write_pos;
	labcomm_decoder_decode_one(test_dec);
	test_enc_ctx->write_pos = 0;

	labcomm_encoder_register_firefly_protocol_channel_response(test_enc);
	test_dec_ctx->enc_data = test_enc_ctx->buf;
	test_dec_ctx->size = test_enc_ctx->write_pos;
	labcomm_decoder_decode_one(test_dec);
	test_enc_ctx->write_pos = 0;

	labcomm_encoder_register_firefly_protocol_channel_ack(test_enc);
	test_dec_ctx->enc_data = test_enc_ctx->buf;
	test_dec_ctx->size = test_enc_ctx->write_pos;
	labcomm_decoder_decode_one(test_dec);
	test_enc_ctx->write_pos = 0;

	labcomm_encoder_register_firefly_protocol_channel_close(test_enc);
	test_dec_ctx->enc_data = test_enc_ctx->buf;
	test_dec_ctx->size = test_enc_ctx->write_pos;
	labcomm_decoder_decode_one(test_dec);
	test_enc_ctx->write_pos = 0;

	labcomm_encoder_register_firefly_protocol_ack(test_enc);
	test_dec_ctx->enc_data = test_enc_ctx->buf;
	test_dec_ctx->size = test_enc_ctx->write_pos;
	labcomm_decoder_decode_one(test_dec);
	test_enc_ctx->write_pos = 0;
	return 0;
}

int clean_labcomm_test_enc_dec()
{
	labcomm_encoder_free(test_enc);
	test_enc = NULL;
	free(test_enc_ctx->buf);
	free(test_enc_ctx);
	test_enc_ctx = NULL;
	free(test_dec_ctx);
	labcomm_decoder_free(test_dec);
	return 0;
}
