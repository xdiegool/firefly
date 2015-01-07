#include "test/proto_helper.h"

#include <stdlib.h>
#include <stdio.h>
#include <labcomm.h>
#include <labcomm_ioctl.h>
#include <labcomm_default_memory.h>
#include "CUnit/Basic.h"
#include <stdbool.h>

#include <gen/firefly_protocol.h>
#include <gen/test.h>
#include <utils/firefly_errors.h>
#include <utils/firefly_event_queue.h>

#include "test/labcomm_static_buffer_writer.h"
#include "test/labcomm_static_buffer_reader.h"
#include "test/test_labcomm_utils.h"
#include "utils/cppmacros.h"
#include "test/event_helper.h"

struct labcomm_decoder *test_dec = NULL;
struct labcomm_encoder *test_enc;

unsigned char data_sample_data[DATA_SAMPLE_DATA_SIZE];
firefly_protocol_data_sample data_sample;
firefly_protocol_channel_request channel_request;
firefly_protocol_channel_response channel_response;
firefly_protocol_channel_ack channel_ack;
firefly_protocol_channel_close channel_close;
firefly_protocol_ack ack;
firefly_protocol_channel_restrict_request restrict_request;
firefly_protocol_channel_restrict_ack restrict_ack;

bool received_data_sample = false;
bool received_channel_request = false;
bool received_channel_response = false;
bool received_channel_ack = false;
bool received_channel_close = false;
bool received_ack = false;
bool received_restrict_request = false;
bool received_restrict_ack = false;
bool received_important = false;
bool conn_ack_called = false;

struct tmp_data conn_open_write;
struct tmp_data conn_recv_write;
bool conn_recv_accept_called = false;

/* struct firefly_connection_actions conn_actions = { */
/* 	.channel_recv		= ch_ac, */
/* 	.channel_opened		= ch_op, */
/* 	.channel_rejected	= NULL, */
/* 	.channel_closed		= ch_cl, */
/* 	.channel_restrict	= NULL, */
/* 	.channel_restrict_info	= NULL */
/* }; */

/* firefly_channel_is_open_f ch_op,
firefly_channel_closed_f ch_cl,
firefly_channel_accept_f ch_acc, */

static int test_conn_open(struct firefly_connection *conn)
{
	struct firefly_connection **res = conn->transport->context;
	*res = conn;
	res = NULL;
	return 0;
}
static int test_conn_close(struct firefly_connection *conn)
{
	free(conn->transport);
	return 0;
}

struct firefly_connection *setup_test_conn_new(
	       struct firefly_connection_actions *conn_actions,
	       struct firefly_event_queue *eq)
{
	struct firefly_connection *conn;
	struct firefly_transport_connection *test_trsp_conn =
		malloc(sizeof(*test_trsp_conn));
	test_trsp_conn->write = transport_write_test_decoder;
	test_trsp_conn->ack = transport_ack_test;
	test_trsp_conn->open = test_conn_open;
	test_trsp_conn->close = test_conn_close;
	test_trsp_conn->context = &conn;
	int res = firefly_connection_open(conn_actions, NULL, eq, test_trsp_conn, NULL);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	if (conn == NULL) {
		CU_FAIL_FATAL("Could not create connection.\n");
	}
	// Set some stuff that firefly_init_conn doesn't do
	// TODO: Fix this when labcomm gets error handling back
	/* labcomm_register_error_handler_encoder(conn->transport_encoder,*/
	/*                 handle_labcomm_error);*/
	/* labcomm_register_error_handler_decoder(conn->transport_decoder,*/
	/*                 handle_labcomm_error);*/
	return conn;
}

bool chan_opened_called = false;
void chan_opened_mock(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	chan_opened_called = true;
}

void transport_ack_test(unsigned char id, struct firefly_connection *conn)
{
	UNUSED_VAR(conn);
	CU_ASSERT_EQUAL(id, IMPORTANT_ID);
	conn_ack_called = true;
}

void transport_write_test_decoder(unsigned char *data, size_t size,
					  struct firefly_connection *conn, bool important,
					   unsigned char *id)
{
	UNUSED_VAR(conn);
	received_important = important;
	if (received_important) {
		CU_ASSERT_PTR_NOT_NULL_FATAL(id);
		*id = IMPORTANT_ID;
	}
	CU_ASSERT_PTR_NOT_NULL_FATAL(data);
	CU_ASSERT_FATAL(size > 0);
	labcomm_decoder_ioctl(test_dec, LABCOMM_IOCTL_READER_SET_BUFFER,
			data, size);
	labcomm_decoder_decode_one(test_dec);
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

void test_handle_restrict_request(firefly_protocol_channel_restrict_request *d,
		void *ctx)
{
	UNUSED_VAR(ctx);
	memcpy(&restrict_request, d, sizeof(*d));
	received_restrict_request = true;
}

void test_handle_restrict_ack(firefly_protocol_channel_restrict_ack *d, void *ctx)
{
	UNUSED_VAR(ctx);
	memcpy(&restrict_ack, d, sizeof(*d));
	received_restrict_ack = true;
}

int init_labcomm_test_enc_dec_custom(struct labcomm_reader *test_r,
		struct labcomm_writer *test_w)
{
	test_enc = labcomm_encoder_new(test_w, NULL, labcomm_default_memory,
			NULL);
	if (test_enc == NULL) {
		return 1;
	}
	// TODO: Fix when labcomm gets error handling back
	/* labcomm_register_error_handler_encoder(test_enc, handle_labcomm_error);*/

	test_dec = labcomm_decoder_new(test_r, NULL, labcomm_default_memory,
			NULL);
	if (test_dec == NULL) {
		CU_FAIL("Test decoder was null\n");
	}
	// TODO: Fix when labcomm gets error handling back
	/* labcomm_register_error_handler_decoder(test_dec, handle_labcomm_error);*/

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
	labcomm_decoder_register_firefly_protocol_channel_restrict_request(test_dec,
						test_handle_restrict_request, NULL);
	labcomm_decoder_register_firefly_protocol_channel_restrict_ack(test_dec,
						test_handle_restrict_ack, NULL);

	void *buffer;
	size_t buffer_size;
	labcomm_encoder_register_firefly_protocol_data_sample(test_enc);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buffer, &buffer_size);
	labcomm_decoder_ioctl(test_dec, LABCOMM_IOCTL_READER_SET_BUFFER,
			buffer, buffer_size);
	labcomm_decoder_decode_one(test_dec);
	free(buffer);

	labcomm_encoder_register_firefly_protocol_channel_request(test_enc);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buffer, &buffer_size);
	labcomm_decoder_ioctl(test_dec, LABCOMM_IOCTL_READER_SET_BUFFER,
			buffer, buffer_size);
	labcomm_decoder_decode_one(test_dec);
	free(buffer);

	labcomm_encoder_register_firefly_protocol_channel_response(test_enc);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buffer, &buffer_size);
	labcomm_decoder_ioctl(test_dec, LABCOMM_IOCTL_READER_SET_BUFFER,
			buffer, buffer_size);
	labcomm_decoder_decode_one(test_dec);
	free(buffer);

	labcomm_encoder_register_firefly_protocol_channel_ack(test_enc);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buffer, &buffer_size);
	labcomm_decoder_ioctl(test_dec, LABCOMM_IOCTL_READER_SET_BUFFER,
			buffer, buffer_size);
	labcomm_decoder_decode_one(test_dec);
	free(buffer);

	labcomm_encoder_register_firefly_protocol_channel_close(test_enc);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buffer, &buffer_size);
	labcomm_decoder_ioctl(test_dec, LABCOMM_IOCTL_READER_SET_BUFFER,
			buffer, buffer_size);
	labcomm_decoder_decode_one(test_dec);
	free(buffer);

	labcomm_encoder_register_firefly_protocol_ack(test_enc);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buffer, &buffer_size);
	labcomm_decoder_ioctl(test_dec, LABCOMM_IOCTL_READER_SET_BUFFER,
			buffer, buffer_size);
	labcomm_decoder_decode_one(test_dec);
	free(buffer);

	labcomm_encoder_register_firefly_protocol_channel_restrict_request(test_enc);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buffer, &buffer_size);
	labcomm_decoder_ioctl(test_dec, LABCOMM_IOCTL_READER_SET_BUFFER,
			buffer, buffer_size);
	labcomm_decoder_decode_one(test_dec);
	free(buffer);

	labcomm_encoder_register_firefly_protocol_channel_restrict_ack(test_enc);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buffer, &buffer_size);
	labcomm_decoder_ioctl(test_dec, LABCOMM_IOCTL_READER_SET_BUFFER,
			buffer, buffer_size);
	labcomm_decoder_decode_one(test_dec);
	free(buffer);

	return 0;
}

int init_labcomm_test_enc_dec()
{
	struct labcomm_writer *test_w;
	struct labcomm_reader *test_r;

	test_r = labcomm_static_buffer_reader_new(labcomm_default_memory);
	test_w = labcomm_static_buffer_writer_new();
	return init_labcomm_test_enc_dec_custom(test_r, test_w);
}

int clean_labcomm_test_enc_dec()
{
	labcomm_encoder_free(test_enc);
	test_enc = NULL;
	labcomm_decoder_free(test_dec);
	return 0;
}
