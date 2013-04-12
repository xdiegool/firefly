#include "test/test_proto_chan.h"

#include "CUnit/Basic.h"

#include <labcomm.h>
#include <test/labcomm_mem_writer.h>
#include <test/labcomm_mem_reader.h>
#include <labcomm_fd_reader_writer.h>

#include <stdbool.h>

#include <gen/firefly_protocol.h>
#include <gen/test.h>
#include <utils/firefly_errors.h>
#include <protocol/firefly_protocol.h>
#include <utils/firefly_event_queue.h>

#include "test/proto_helper.h"
#include "protocol/firefly_protocol_private.h"
#include "test/test_labcomm_utils.h"
#include "utils/cppmacros.h"

#define REMOTE_CHAN_ID (2) // Chan id used by all simulated remote channels.
#define ENCODER_WRITE_BUF_SIZE (128)
#define DATA_SAMPLE_DATA_SIZE (128)

extern struct labcomm_decoder *test_dec;
extern labcomm_mem_reader_context_t *test_dec_ctx;

struct labcomm_encoder *test_enc;
labcomm_mem_writer_context_t *test_enc_ctx;

static unsigned char data_sample_data[DATA_SAMPLE_DATA_SIZE];
static firefly_protocol_data_sample data_sample;
static firefly_protocol_channel_request channel_request;
static firefly_protocol_channel_response channel_response;
static firefly_protocol_channel_ack channel_ack;
static firefly_protocol_channel_close channel_close;

static bool received_data_sample = false;
static bool received_channel_request = false;
static bool received_channel_response = false;
static bool received_channel_ack = false;
static bool received_channel_close = false;

static void test_handle_data_sample(firefly_protocol_data_sample *d, void *ctx)
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
static void test_handle_channel_request(firefly_protocol_channel_request *d, void *ctx)
{
	UNUSED_VAR(ctx);
	memcpy(&channel_request, d, sizeof(firefly_protocol_channel_request));
	received_channel_request = true;
}
static void test_handle_channel_response(firefly_protocol_channel_response *d, void *ctx)
{
	UNUSED_VAR(ctx);
	memcpy(&channel_response, d, sizeof(firefly_protocol_channel_response));
	received_channel_response = true;
}
static void test_handle_channel_ack(firefly_protocol_channel_ack *d, void *ctx)
{
	UNUSED_VAR(ctx);
	memcpy(&channel_ack, d, sizeof(firefly_protocol_channel_ack));
	received_channel_ack = true;
}
static void test_handle_channel_close(firefly_protocol_channel_close *d, void *ctx)
{
	UNUSED_VAR(ctx);
	memcpy(&channel_close, d, sizeof(firefly_protocol_channel_close));
	received_channel_close = true;
}

int init_suit_proto_chan()
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
	return 0; // Success.
}

int clean_suit_proto_chan()
{
	labcomm_encoder_free(test_enc);
	test_enc = NULL;
	free(test_enc_ctx->buf);
	free(test_enc_ctx);
	test_enc_ctx = NULL;
	free(test_dec_ctx);
	labcomm_decoder_free(test_dec);
	return 0; // Success.
}

extern bool chan_opened_called;

#define MAX_TESTED_ID (10)

void test_next_channel_id()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	struct firefly_connection *conn = setup_test_conn_new(NULL,
			NULL, NULL, eq);
	for (int i = 0; i < MAX_TESTED_ID; ++i) {
		struct firefly_channel *ch = firefly_channel_new(conn);
		CU_ASSERT_EQUAL(i, ch->local_id);
		firefly_channel_free(ch);
	}
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

void test_get_streams()
{
	// Construct encoder.
	labcomm_mem_writer_context_t *wmcontext =
			labcomm_mem_writer_context_t_new(0, 0, NULL);
	struct labcomm_encoder *e_encoder = labcomm_encoder_new(
			labcomm_mem_writer, wmcontext);
	if (e_encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder.");
	}
	labcomm_register_error_handler_encoder(e_encoder, handle_labcomm_error);

	// Construct decoder.
	struct labcomm_mem_reader_context_t rmcontext;
	struct labcomm_decoder *e_decoder = labcomm_decoder_new(
			labcomm_mem_reader, &rmcontext);
	if (e_decoder == NULL) {
		CU_FAIL("Could not allocate LabComm decoder.");
	}
	labcomm_register_error_handler_decoder(e_decoder, handle_labcomm_error);

	struct firefly_channel chan;
	chan.proto_encoder = e_encoder;
	chan.proto_decoder = e_decoder;

	struct labcomm_encoder *a_encoder = firefly_protocol_get_output_stream(
									&chan);
	struct labcomm_decoder *a_decoder = firefly_protocol_get_input_stream(
									&chan);
	CU_ASSERT_PTR_EQUAL(e_encoder, a_encoder);
	CU_ASSERT_PTR_EQUAL(e_decoder, a_decoder);

	labcomm_encoder_free(e_encoder);
	labcomm_decoder_free(e_decoder);
	labcomm_mem_writer_context_t_free(&wmcontext);
}

void test_get_conn()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	// Init connection and register error handler on encoder and decoder
	struct firefly_connection *conn =
		setup_test_conn_new(chan_opened_mock, NULL, NULL, eq);
	struct firefly_channel *chan = firefly_channel_new(conn);

	struct firefly_connection *get_conn = firefly_channel_get_connection(
			chan);
	CU_ASSERT_PTR_EQUAL(conn, get_conn);
	firefly_channel_free(chan);
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

void test_chan_open()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	// Init connection and register error handler on encoder and decoder
	struct firefly_connection *conn =
		setup_test_conn_new(chan_opened_mock, NULL, NULL, eq);

	// Open a new channel
	firefly_channel_open(conn, NULL);

	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	// Test that a chan open request is sent.
	CU_ASSERT_TRUE(received_channel_request);
	received_channel_request = false;
	CU_ASSERT_EQUAL(conn->chan_list->chan->local_id, channel_request.source_chan_id);
	CU_ASSERT_EQUAL(CHANNEL_ID_NOT_SET, channel_request.dest_chan_id);

	// Simulate that we received a channel response from the other end
	firefly_protocol_channel_response chan_resp;
	chan_resp.source_chan_id = REMOTE_CHAN_ID;
	chan_resp.dest_chan_id = conn->chan_list->chan->local_id;
	chan_resp.ack = true;
	labcomm_encode_firefly_protocol_channel_response(test_enc, &chan_resp);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;

	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	// Test that we sent an ack
	CU_ASSERT_TRUE(received_channel_ack);
	received_channel_ack = false;
	CU_ASSERT_EQUAL(conn->chan_list->chan->local_id, channel_ack.source_chan_id);
	CU_ASSERT_EQUAL(conn->chan_list->chan->remote_id, channel_ack.dest_chan_id);
	CU_ASSERT_EQUAL(conn->chan_list->chan->remote_id, REMOTE_CHAN_ID);
	CU_ASSERT_TRUE(channel_ack.ack);

	// test chan_is_open is called
	CU_ASSERT_TRUE(chan_opened_called);

	// Clean up
	chan_opened_called = false;
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

static bool chan_accept_called = false;
static bool chan_recv_accept_chan(struct firefly_channel *chan)
{
	CU_ASSERT_EQUAL(chan->remote_id, REMOTE_CHAN_ID);
	chan_accept_called = true;
	return true;
}

void test_chan_recv_accept()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	// Init connection and register error handler on encoder and decoder
	struct firefly_connection *conn =
		setup_test_conn_new(chan_opened_mock, NULL,
				chan_recv_accept_chan, eq);

	// Create channel request.
	firefly_protocol_channel_request chan_req;
	chan_req.source_chan_id = REMOTE_CHAN_ID;
	chan_req.dest_chan_id = CHANNEL_ID_NOT_SET;
	// Give channel request data to protocol layer.
	labcomm_encode_firefly_protocol_channel_request(test_enc, &chan_req);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;

	struct firefly_event *ev = firefly_event_pop(eq);
	firefly_event_execute(ev);

	// Test accept called.
	CU_ASSERT_TRUE(chan_accept_called);
	chan_accept_called = false; // Reset value.

	CU_ASSERT_TRUE(received_channel_response);
	received_channel_response = false;
	CU_ASSERT_EQUAL(channel_response.dest_chan_id, REMOTE_CHAN_ID);
	CU_ASSERT_EQUAL(conn->chan_list->chan->remote_id, REMOTE_CHAN_ID);
	CU_ASSERT_EQUAL(conn->chan_list->chan->local_id,
			channel_response.source_chan_id);

	// simulate an ACK is sent from remote
	firefly_protocol_channel_ack ack;
	ack.dest_chan_id = conn->chan_list->chan->local_id;
	ack.source_chan_id = conn->chan_list->chan->remote_id;
	ack.ack = true;
	// Recieving end and got an ack
	labcomm_encode_firefly_protocol_channel_ack(test_enc, &ack);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	ev = firefly_event_pop(eq);
	firefly_event_execute(ev);

	CU_ASSERT_TRUE(chan_opened_called);
	chan_opened_called = false;

	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

static bool chan_recv_reject_chan(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	chan_accept_called = true;
	return false;
}

void test_chan_recv_reject()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	// Setup connection
	struct firefly_connection *conn =
		setup_test_conn_new(chan_opened_mock, NULL,
				       chan_recv_reject_chan, eq);

	// create temporary labcomm encoder to send request.
	firefly_protocol_channel_request chan_req;
	chan_req.dest_chan_id = CHANNEL_ID_NOT_SET;
	chan_req.source_chan_id = REMOTE_CHAN_ID;
	labcomm_encode_firefly_protocol_channel_request(test_enc, &chan_req);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;

	struct firefly_event *ev = firefly_event_pop(eq);
	firefly_event_execute(ev);

	// check accept called
	CU_ASSERT_TRUE(chan_accept_called);
	// check chan_res sent and correctly set
	CU_ASSERT_TRUE(received_channel_response);
	received_channel_response = false;
	CU_ASSERT_FALSE(channel_response.ack);
	CU_ASSERT_EQUAL(channel_response.dest_chan_id, REMOTE_CHAN_ID);
	CU_ASSERT_EQUAL(channel_response.source_chan_id, CHANNEL_ID_NOT_SET);

	// check chan_opened is not called.
	CU_ASSERT_FALSE(chan_opened_called);

	// check channel is destroyed and all related resources dealloced
	CU_ASSERT_PTR_NULL(conn->chan_list);

	// Clean up
	chan_opened_called = false;
	chan_accept_called = false;
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

static bool chan_rejected_called = false;
void chan_open_chan_rejected(struct firefly_connection *conn)
{
	UNUSED_VAR(conn);
	chan_rejected_called = true;
}

void test_chan_open_rejected()
{

	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}

	// Setup connection
	struct firefly_connection *conn =
		setup_test_conn_new(chan_opened_mock, NULL,
				       NULL, eq);
	if (conn == NULL) {
		CU_FAIL("Could not create connection.\n");
	}

	firefly_channel_open(conn, chan_open_chan_rejected);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	CU_ASSERT_TRUE(received_channel_request);
	received_channel_request = false;

	firefly_protocol_channel_response chan_res;
	chan_res.dest_chan_id = conn->chan_list->chan->local_id;
	chan_res.source_chan_id = CHANNEL_ID_NOT_SET;
	chan_res.ack = false;

	labcomm_encode_firefly_protocol_channel_response(test_enc, &chan_res);
	// send response
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	CU_ASSERT_FALSE(received_channel_ack);
	received_channel_ack = false;
	CU_ASSERT_PTR_NULL(conn->chan_list);
	// test resources are destroyed again
	CU_ASSERT_FALSE(chan_opened_called);
	CU_ASSERT_TRUE(chan_rejected_called);

	// Clean up
	chan_rejected_called = false;
	chan_opened_called = false;
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

extern bool conn_recv_accept_called;

extern struct tmp_data conn_open_write;
extern struct tmp_data conn_recv_write;

void test_chan_open_recv()
{
	struct firefly_connection *conn_open;
	struct firefly_connection *conn_recv;

	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	// Init connection to open channel
	conn_open = setup_test_conn_new(chan_opened_mock, NULL,
				   	   chan_open_recv_accept_open, eq);
	conn_open->transport_write = chan_open_recv_write_open;

	// Init connection to receive channel
	conn_recv = setup_test_conn_new(chan_opened_mock, NULL,
					chan_open_recv_accept_recv, eq);
	conn_recv->transport_write = chan_open_recv_write_recv;

	// Init open channel from conn_open
	firefly_channel_open(conn_open, NULL);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	free_tmp_data(&conn_open_write);
	CU_ASSERT_TRUE(conn_recv_accept_called);
	protocol_data_received(conn_open, conn_recv_write.data,
			conn_recv_write.size);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	CU_ASSERT_TRUE(chan_opened_called);
	chan_opened_called = false;

	free_tmp_data(&conn_recv_write);
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	free_tmp_data(&conn_open_write);
	ev = firefly_event_pop(eq);
	firefly_event_execute(ev);
	CU_ASSERT_TRUE(chan_opened_called);
	chan_opened_called = false;

	CU_ASSERT_PTR_NOT_NULL(conn_open->chan_list->chan);
	CU_ASSERT_PTR_NOT_NULL(conn_recv->chan_list->chan);
	CU_ASSERT_EQUAL(conn_recv->chan_list->chan->local_id,
			conn_open->chan_list->chan->remote_id);
	CU_ASSERT_EQUAL(conn_recv->chan_list->chan->remote_id,
			conn_open->chan_list->chan->local_id);

	// Clean up
	chan_opened_called = false;
	firefly_connection_free(&conn_open);
	firefly_connection_free(&conn_recv);
	firefly_event_queue_free(&eq);
}

static bool chan_closed_called = false;
void chan_closed_cb(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	chan_closed_called = true;
}

void test_chan_close()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}

	// Setup connection
	struct firefly_connection *conn =
		setup_test_conn_new(NULL, chan_closed_cb, NULL, eq);

	// create channel
	struct firefly_channel *chan = firefly_channel_new(conn);
	chan->remote_id = REMOTE_CHAN_ID;
	add_channel_to_connection(chan, conn);

	// close channel
	firefly_channel_close(chan);
	// pop event to send channel close
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	// test close packet sent
	CU_ASSERT_TRUE(received_channel_close);
	received_channel_close = false;
	CU_ASSERT_EQUAL(channel_close.dest_chan_id, chan->remote_id);
	CU_ASSERT_EQUAL(channel_close.source_chan_id, chan->local_id);

	// pop event to close channel
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	// test channel state, free'd
	CU_ASSERT_PTR_NULL(conn->chan_list);
	CU_ASSERT_TRUE(chan_closed_called);

	// clean up
	chan_closed_called = false;
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

void test_chan_recv_close()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}

	// Setup connection
	struct firefly_connection *conn =
		setup_test_conn_new(NULL, chan_closed_cb, NULL, eq);

	// create channel
	struct firefly_channel *chan = firefly_channel_new(conn);
	chan->remote_id = REMOTE_CHAN_ID;
	add_channel_to_connection(chan, conn);

	// create close channel packet
	firefly_protocol_channel_close chan_close;
	chan_close.dest_chan_id = conn->chan_list->chan->local_id;
	chan_close.source_chan_id = REMOTE_CHAN_ID;
	labcomm_encode_firefly_protocol_channel_close(test_enc, &chan_close);

	// give packet to protocol
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	// pop and execute event
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ev);
	firefly_event_execute(ev);
	// check callback called.
	CU_ASSERT_TRUE(chan_closed_called);
	// check channel state
	CU_ASSERT_PTR_NULL(conn->chan_list);

	// clean up
	chan_closed_called = false;
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

static bool sent_app_data = false;
void handle_test_test_var(test_test_var *data, void *ctx)
{
	UNUSED_VAR(ctx);
	CU_ASSERT_EQUAL(*data, 1);
	sent_app_data = true;
}

void test_send_app_data()
{
	test_test_var app_test_data = 1;
	struct firefly_event *ev;

	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}

	// Setup connection
	struct firefly_connection *conn =
		setup_test_conn_new(NULL, chan_closed_cb, NULL, eq);

	struct firefly_channel *ch = firefly_channel_new(conn);
	ch->remote_id = REMOTE_CHAN_ID;
	add_channel_to_connection(ch, conn);

	struct labcomm_mem_reader_context_t *test_dec_ctx_2;
	test_dec_ctx_2 = malloc(sizeof(labcomm_mem_reader_context_t));
	if (test_dec_ctx_2 == NULL) {
		CU_FAIL("Test decoder context was null\n");
	}
	struct labcomm_decoder *test_dec_2 =
		labcomm_decoder_new(labcomm_mem_reader, test_dec_ctx_2);
	labcomm_register_error_handler_decoder(test_dec_2,
			handle_labcomm_error);
	labcomm_decoder_register_test_test_var(test_dec_2,
			handle_test_test_var, NULL);

	struct labcomm_encoder *ch_enc = firefly_protocol_get_output_stream(ch);
	labcomm_encoder_register_test_test_var(ch_enc);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	CU_ASSERT_TRUE(received_data_sample);
	received_data_sample = false;
	CU_ASSERT_EQUAL(ch->local_id, data_sample.src_chan_id);
	CU_ASSERT_EQUAL(ch->remote_id, data_sample.dest_chan_id);

	test_dec_ctx_2->enc_data = data_sample.app_enc_data.a;
	test_dec_ctx_2->size = data_sample.app_enc_data.n_0;
	labcomm_decoder_decode_one(test_dec_2);

	labcomm_encode_test_test_var(ch_enc, &app_test_data);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	CU_ASSERT_TRUE(received_data_sample);
	received_data_sample = false;

	test_dec_ctx_2->enc_data = data_sample.app_enc_data.a;
	test_dec_ctx_2->size = data_sample.app_enc_data.n_0;
	labcomm_decoder_decode_one(test_dec_2);

	CU_ASSERT_TRUE(sent_app_data);
	sent_app_data = false;

	firefly_connection_free(&conn);
	labcomm_decoder_free(test_dec_2);
	free(test_dec_ctx_2);
	firefly_event_queue_free(&eq);
}

void test_recv_app_data()
{
	struct firefly_event *ev;
	struct firefly_event_queue *eq =
		firefly_event_queue_new(firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}

	// Setup connection
	struct firefly_connection *conn =
		setup_test_conn_new(NULL, NULL, NULL, eq);
	struct firefly_channel *ch = firefly_channel_new(conn);
	if (ch == NULL)
		CU_FAIL("Could not create channel");
	ch->remote_id = REMOTE_CHAN_ID;
	add_channel_to_connection(ch, conn);

	struct labcomm_decoder *ch_dec = firefly_protocol_get_input_stream(ch);
	labcomm_decoder_register_test_test_var(ch_dec, handle_test_test_var,
			NULL);

	// Create app data packet
	test_test_var app_pkt;
	app_pkt = 1;
	struct encoded_packet *ep_app = create_encoded_packet(
			labcomm_encoder_register_test_test_var,
			(lc_encode_f) labcomm_encode_test_test_var, &app_pkt);

	// create protocol sample with app signature
	firefly_protocol_data_sample proto_sign_pkt;
	proto_sign_pkt.src_chan_id = REMOTE_CHAN_ID;
	proto_sign_pkt.dest_chan_id = ch->local_id;
	proto_sign_pkt.important = true;
	proto_sign_pkt.app_enc_data.a = ep_app->sign;
	proto_sign_pkt.app_enc_data.n_0 = ep_app->ssize;

	labcomm_encode_firefly_protocol_data_sample(test_enc, &proto_sign_pkt);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	// execute the generated events
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	// create protocol sample with app data
	firefly_protocol_data_sample proto_data_pkt;
	proto_data_pkt.src_chan_id = REMOTE_CHAN_ID;
	proto_data_pkt.dest_chan_id = ch->local_id;
	proto_data_pkt.important = true;
	proto_data_pkt.app_enc_data.a = ep_app->data;
	proto_data_pkt.app_enc_data.n_0 = ep_app->dsize;

	labcomm_encode_firefly_protocol_data_sample(test_enc, &proto_data_pkt);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	// execute the generated events
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	// check if app data was received
	CU_ASSERT_TRUE(sent_app_data);
	sent_app_data = false;

	// clean up
	encoded_packet_free(ep_app);
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}


/* Big test using mock transport layer. */
struct data_space {
	unsigned char *data;
	struct data_space *next;
	size_t data_size;
};

struct data_space *data_space_new()
{
	struct data_space *tmp;

	if ((tmp = malloc(sizeof(*tmp))) != NULL) {
		tmp->data = NULL;
		tmp->next = NULL;
		tmp->data_size = 0;
	}

	return tmp;
}

static int n_packets_in_data_space(struct data_space *space)
{
	int n = 0;

	while (space) {
		space = space->next;
		n++;
	}

	return n;
}

static struct data_space *space_from_conn[2];

/* transport_write_f callback*/
void trans_w(struct data_space **space,
			 unsigned char *data,
			 size_t data_size,
			 struct firefly_connection *conn,
			 bool important,
			 unsigned char *id)
{
	UNUSED_VAR(conn);
	UNUSED_VAR(important);
	UNUSED_VAR(id);
	struct data_space *tmp;

	if (*space == NULL) {
		tmp = *space = data_space_new();
	} else {
		tmp = *space;
		while (tmp->next)
			tmp = tmp->next;
		tmp->next = data_space_new();
		tmp = tmp->next;		/* check? */
	}
	/* now on a new space, pos last. */
	if ((tmp->data = malloc(data_size)) == NULL) {
		CU_FAIL("malloc failed!");
	}
	memcpy(tmp->data, data, data_size);
	tmp->data_size = data_size;
	//labcomm_decoder_decode_one(conn->transport_decoder); // huh?
}

void trans_w_from_conn_0(unsigned char *data, size_t data_size,
					 struct firefly_connection *conn, bool important,
					 unsigned char *id)
{
	trans_w(&space_from_conn[0], data, data_size, conn, important, id);
}

void trans_w_from_conn_1(unsigned char *data, size_t data_size,
					 struct firefly_connection *conn, bool important,
					 unsigned char *id)
{
	trans_w(&space_from_conn[1], data, data_size, conn, important, id);
}

size_t read_connection_mock(struct firefly_connection *conns[], int conn_n)
{
	struct data_space **space;
	struct data_space *read_pkg;
	size_t read_size;

	/* Connection 0 reads where connection 1 writes and vice versa. */
	space = (conn_n == 0) ? &space_from_conn[1] : &space_from_conn[0];
	read_pkg = *space;
	if (*space == NULL) {
		return 0;
	}
	*space = (*space)->next;
	protocol_data_received(conns[conn_n], read_pkg->data,
			read_pkg->data_size);
	read_size = read_pkg->data_size;
	free(read_pkg->data);
	free(read_pkg);

	return read_size;
}

static struct firefly_channel *new_chan;
static int n_chan_opens;

static bool should_accept_chan(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	return true;
}
void chan_was_opened(struct firefly_channel *chan)
{
	new_chan = chan;
	n_chan_opens++;
}

void chan_was_closed(struct firefly_channel *chan) 
{
	UNUSED_VAR(chan);
}

void handle_ttv(test_test_var *ttv, void *cont)
{
	*((test_test_var *) cont) = *ttv;
}

/* we need two connections for this test */
struct firefly_connection *setup_conn(int conn_n,
		struct firefly_event_queue *eqs[])
{
	struct firefly_connection *tcon;

	if (conn_n < 0 || conn_n > 1) {
		CU_FAIL("This test have functions to handle *2* connections!");
	}

	transport_write_f writer = NULL;
	switch (conn_n) {		/* TODO: Expandable later(?) */
	case 0:
		writer = trans_w_from_conn_0;
		break;
	case 1:
		writer = trans_w_from_conn_1;
		break;
	}

	tcon = firefly_connection_new(chan_was_opened, chan_was_closed,
					   should_accept_chan, writer,
					   eqs[conn_n], NULL, NULL);

	if (tcon == NULL) {
		CU_FAIL("Could not create channel");
	}
	void *err_cb;
	err_cb = handle_labcomm_error;
	//err_cb = NULL;
	labcomm_register_error_handler_encoder(tcon->transport_encoder,
					   	   err_cb);
	labcomm_register_error_handler_decoder(tcon->transport_decoder,
						   err_cb);

	return tcon;
}

void test_transmit_app_data_over_mock_trans_layer()
{
	const int n_conn = 2;
	struct firefly_event_queue *event_queues[n_conn];
	struct firefly_connection *connections[n_conn];

	/* construct and configure queues with callback */
	for (int i = 0; i < n_conn; i++) {
		event_queues[i] = firefly_event_queue_new(firefly_event_add,
				NULL);
		if (!event_queues[i])
			CU_FAIL("Could not allocate queue");
	}

	/* setup connections */
	for (int i = 0; i < n_conn; i++) {
		connections[i] = setup_conn(i, event_queues);
	}

	/* Actually do stuff. */
	/* Initial values of event queues */
	for (int i = 0; i < n_conn; i++) {
		CU_ASSERT_EQUAL(firefly_event_queue_length(
					connections[i]->event_queue), 0);
	}

	/* Chan 0 wants to open a connection... */
	/* TODO: Might want to check the callback too... */
	firefly_channel_open(connections[0], NULL);
	CU_ASSERT_EQUAL(firefly_event_queue_length(
				connections[0]->event_queue), 1);
	struct firefly_event *ev = firefly_event_pop(
			connections[0]->event_queue);
	CU_ASSERT_EQUAL(firefly_event_queue_length(
				connections[0]->event_queue), 0);
	firefly_event_execute(ev);
	CU_ASSERT_EQUAL(firefly_event_queue_length(
				connections[0]->event_queue), 0);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[0]), 1);

	/*
	 * Connection 1 will respond to the request by reading what connection 0
	 * has written. Connection 0 will read signatures.
	 */
	int cnt = 0;
	while (read_connection_mock(connections, 1))
		cnt++;
	CU_ASSERT_EQUAL(cnt, 1);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[1]->event_queue),
			1);
	ev = firefly_event_pop(connections[1]->event_queue);
	firefly_event_execute(ev);

	/* Chan 1 should have written chan_req_resp. Read it. */
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[0]), 0);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[1]), 1);
	read_connection_mock(connections, 0);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[1]), 0);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[0]->event_queue),
			1);
	ev = firefly_event_pop(connections[0]->event_queue); /* recv_resp */
	firefly_event_execute(ev);

	/* No new events are spawned, but an ack in data space. */
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[0]), 1); // Ack
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[0]->event_queue),
			0);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[1]->event_queue),
			0);

	/* process ack */
	read_connection_mock(connections, 1);
	CU_ASSERT_EQUAL(n_chan_opens, 1);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[1]), 0);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[0]), 0);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[0]->event_queue),
			0);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[1]->event_queue),
			1);
	struct firefly_channel *channels[2];
	channels[0] = new_chan;
	ev = firefly_event_pop(connections[1]->event_queue); /* recv_resp */
	firefly_event_execute(ev);

	/* process other one */
	read_connection_mock(connections, 0);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[1]), 0);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[0]), 0);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[0]->event_queue),
			0);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[1]->event_queue),
			0);
	channels[1] = new_chan;

	CU_ASSERT_EQUAL(n_chan_opens, 2);
	CU_ASSERT_EQUAL(firefly_number_channels_in_connection(connections[0]),
			1);
	CU_ASSERT_EQUAL(firefly_number_channels_in_connection(connections[1]),
			1);

	/* get coders */
	struct labcomm_encoder *encoders[2];
	struct labcomm_decoder *decoders[2];
	encoders[0] = firefly_protocol_get_output_stream(channels[0]);
	encoders[1] = firefly_protocol_get_output_stream(channels[1]);
	decoders[0] = firefly_protocol_get_input_stream(channels[0]);
	decoders[1] = firefly_protocol_get_input_stream(channels[1]);
	CU_ASSERT_PTR_NOT_NULL(encoders[0]);
	CU_ASSERT_PTR_NOT_NULL(encoders[1]);
	CU_ASSERT_PTR_NOT_NULL(decoders[0]);
	CU_ASSERT_PTR_NOT_NULL(decoders[1]);

	test_test_var contexts[2];
	contexts[0] = -1;
	contexts[1] = -1;

	labcomm_decoder_register_test_test_var(decoders[0], &handle_ttv,
			&(contexts[0]));

	labcomm_decoder_register_test_test_var(decoders[1], &handle_ttv,
			&(contexts[1]));

	labcomm_encoder_register_test_test_var(encoders[0]);
	labcomm_encoder_register_test_test_var(encoders[1]);

	/*
	 * The new addition of atype should spawn an event. There will
	 * however be no packet yet.
	 */
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[1]), 0);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[0]), 0);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[0]->event_queue),
			1);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[1]->event_queue),
			1);

	/* Execute event to send the data */
	for (int i = 0; i < 2; i++) {
		ev = firefly_event_pop(connections[i]->event_queue);
		firefly_event_execute(ev);
		/* The event is consumed and the data is written. */
		CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[i]), 1);
		CU_ASSERT_EQUAL(firefly_event_queue_length(
					connections[i]->event_queue), 0);
	}
	// Because this it the upper layer, an event will be spavned by reading.
	for (int i = 0; i < 2; i++) {
		read_connection_mock(connections, i);
		CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[1-i]),
				0);
		CU_ASSERT_EQUAL(firefly_event_queue_length(
					connections[i]->event_queue), 1);
	}
	/* Execute the event to get the ttv registerd on the upper decoder. */
	for (int i = 0; i < 2; i++) {
		ev = firefly_event_pop(connections[i]->event_queue);
		firefly_event_execute(ev);
		/* The event is consumed and the data is written. */
		CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[i]), 0);
		CU_ASSERT_EQUAL(firefly_event_queue_length(
					connections[i]->event_queue), 0);
	}
	/*
	 * The channel is now ready for transmissions of *real* app data.
	 * Everything is empty.
	 */

	test_test_var sent_app_data = 84;

	/* Spawns event, no data yet */
	labcomm_encode_test_test_var(encoders[0], &sent_app_data);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[0]->event_queue),
			1);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[0]), 0);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[1]->event_queue),
			0);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[1]), 0);

	/* Execute to send. */
	ev = firefly_event_pop(connections[0]->event_queue);
	firefly_event_execute(ev);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[0]->event_queue),
			0);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[0]), 1);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[1]->event_queue),
			0);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[1]), 0);

	/* Let other chan (1) read */
	read_connection_mock(connections, 1);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[0]), 0);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[1]->event_queue),
			1);
	ev = firefly_event_pop(connections[1]->event_queue);
	firefly_event_execute(ev);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[1]->event_queue),
			0);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[0]), 0);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[0]->event_queue),
			0);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[1]), 0);


	test_test_var recv_app_data = contexts[1];
	CU_ASSERT_EQUAL(recv_app_data, sent_app_data);
	/* Sent appdata from 0 -> 1 */

	/* Let conn 0 init closeing of channel */
	firefly_channel_close(channels[0]);
	/* One event to remove chan locally and one to send close packet... */
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[0]->event_queue),
			2);
	ev = firefly_event_pop(connections[0]->event_queue);
	firefly_event_execute(ev);
	/* ...and data to tell the other party. */
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[0]), 1);
	ev = firefly_event_pop(connections[0]->event_queue);
	firefly_event_execute(ev);
	/* No new event should be spawned. */
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[0]->event_queue),
			0);
	/* No more data should be created by executing the event. */
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[0]), 1);
	/* The channel is already gone on this end. */
	CU_ASSERT_EQUAL(firefly_number_channels_in_connection(connections[0]),
			0);

	/* Let conn read the news of the termination */
	read_connection_mock(connections, 1);
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[1]->event_queue),
			1);
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[1]), 0);
	ev = firefly_event_pop(connections[1]->event_queue);
	firefly_event_execute(ev);
	/* No new events... */
	CU_ASSERT_EQUAL(firefly_event_queue_length(connections[1]->event_queue),
			0);
	/* ...or data. */
	CU_ASSERT_EQUAL(n_packets_in_data_space(space_from_conn[1]), 0);
	/* Both channels are now shut down. */
	CU_ASSERT_EQUAL(firefly_number_channels_in_connection(connections[1]),
			0);

	/* cleanup */
	for (int i = 0; i < n_conn; i++) {
		firefly_connection_free(&connections[i]);
		firefly_event_queue_free(&event_queues[i]);
	}
	for (size_t i = 0;
		(i < sizeof(space_from_conn) / sizeof(*space_from_conn)); i++) {
		struct data_space *tmp = space_from_conn[i];
		while (tmp) {
			struct data_space *next =  tmp->next;
			free(tmp->data);
			free(tmp);
			tmp = next;
		}
	}
}

bool chan_accept_mock(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	chan_accept_called = true;
	return true;
}

static bool open_chan1_var_received = false;
static bool open_chan2_var_received = false;
static bool recv_chan1_var_received = false;
static bool recv_chan2_var_received = false;
void handle_test_var_open1(test_test_var *var, void *ctx)
{
	UNUSED_VAR(ctx);
	CU_ASSERT_EQUAL(*var, 3);
	open_chan1_var_received = true;
}
void handle_test_var_open2(test_test_var *var, void *ctx)
{
	UNUSED_VAR(ctx);
	CU_ASSERT_EQUAL(*var, 4);
	open_chan2_var_received = true;
}
void handle_test_var_recv1(test_test_var *var, void *ctx)
{
	UNUSED_VAR(ctx);
	CU_ASSERT_EQUAL(*var, 1);
	recv_chan1_var_received = true;
}
void handle_test_var_recv2(test_test_var *var, void *ctx)
{
	UNUSED_VAR(ctx);
	CU_ASSERT_EQUAL(*var, 2);
	recv_chan2_var_received = true;
}

void test_chan_open_close_multiple()
{
	struct firefly_connection *conn_open;
	struct firefly_connection *conn_recv;

	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	// Init connection to open channel
	conn_open = setup_test_conn_new(chan_opened_mock, NULL,
							chan_accept_mock, eq);
	conn_open->transport_write = chan_open_recv_write_open;

	// Init connection to receive channel
	conn_recv = setup_test_conn_new(chan_opened_mock, NULL,
							chan_accept_mock, eq);
	conn_recv->transport_write = chan_open_recv_write_recv;

	// Init open channel from conn_open
	firefly_channel_open(conn_open, NULL);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	free_tmp_data(&conn_open_write);
	CU_ASSERT_TRUE(chan_accept_called);
	chan_accept_called = false;

	protocol_data_received(conn_open, conn_recv_write.data,
			conn_recv_write.size);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	CU_ASSERT_TRUE(chan_opened_called);
	chan_opened_called = false;

	free_tmp_data(&conn_recv_write);
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	free_tmp_data(&conn_open_write);
	ev = firefly_event_pop(eq);
	firefly_event_execute(ev);
	CU_ASSERT_TRUE(chan_opened_called);
	chan_opened_called = false;

	// Test first channel is created with correct id's
	CU_ASSERT_PTR_NOT_NULL(conn_open->chan_list->chan);
	CU_ASSERT_PTR_NOT_NULL(conn_recv->chan_list->chan);
	CU_ASSERT_EQUAL(conn_recv->chan_list->chan->local_id,
			conn_open->chan_list->chan->remote_id);
	CU_ASSERT_EQUAL(conn_recv->chan_list->chan->remote_id,
			conn_open->chan_list->chan->local_id);
	int chan_id_conn_recv = conn_recv->chan_list->chan->local_id;
	int chan_id_conn_open = conn_open->chan_list->chan->local_id;

	// Init open channel from conn_recv
	firefly_channel_open(conn_recv, NULL);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	protocol_data_received(conn_open, conn_recv_write.data,
			conn_recv_write.size);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	free_tmp_data(&conn_recv_write);

	CU_ASSERT_TRUE(chan_accept_called);
	chan_accept_called = false;
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	free_tmp_data(&conn_open_write);
	CU_ASSERT_TRUE(chan_opened_called);
	chan_opened_called = false;

	protocol_data_received(conn_open, conn_recv_write.data,
			conn_recv_write.size);
	free_tmp_data(&conn_recv_write);
	ev = firefly_event_pop(eq);
	firefly_event_execute(ev);
	CU_ASSERT_TRUE(chan_opened_called);
	chan_opened_called = false;

	// Test second channel is created with correct id's
	// assumes the second channel is placed first in the list
	// The test of app data transfer depends on the order of channels
	// asserted
	// here, if these assertions ever change it must be reflected in the app
	// data transfer test.
	CU_ASSERT_PTR_NOT_NULL(conn_open->chan_list->next->chan);
	CU_ASSERT_PTR_NOT_NULL(conn_recv->chan_list->next->chan);
	CU_ASSERT_NOT_EQUAL(conn_recv->chan_list->chan->local_id,
			chan_id_conn_recv);
	CU_ASSERT_NOT_EQUAL(conn_open->chan_list->chan->local_id,
			chan_id_conn_open);
	CU_ASSERT_EQUAL(conn_recv->chan_list->chan->local_id,
			conn_open->chan_list->chan->remote_id);
	CU_ASSERT_EQUAL(conn_recv->chan_list->chan->remote_id,
			conn_open->chan_list->chan->local_id);

	// Setup app data transfer, assumes the channels are placed identically
	// in both connections, ie the first channel in conn_open corresponds to
	// the first channel in conn_recv.
	struct labcomm_encoder *open_chan1_enc =
		firefly_protocol_get_output_stream( conn_open->chan_list->chan);
	struct labcomm_decoder *open_chan1_dec =
		firefly_protocol_get_input_stream( conn_open->chan_list->chan);

	struct labcomm_encoder *open_chan2_enc =
		firefly_protocol_get_output_stream(
				conn_open->chan_list->next->chan);
	struct labcomm_decoder *open_chan2_dec =
		firefly_protocol_get_input_stream(
				conn_open->chan_list->next->chan);

	struct labcomm_encoder *recv_chan1_enc =
		firefly_protocol_get_output_stream( conn_recv->chan_list->chan);
	struct labcomm_decoder *recv_chan1_dec =
		firefly_protocol_get_input_stream( conn_recv->chan_list->chan);

	struct labcomm_encoder *recv_chan2_enc =
		firefly_protocol_get_output_stream(
				conn_recv->chan_list->next->chan);
	struct labcomm_decoder *recv_chan2_dec =
		firefly_protocol_get_input_stream(
				conn_recv->chan_list->next->chan);

	// Register type on the decoders
	labcomm_decoder_register_test_test_var(open_chan1_dec,
			handle_test_var_open1, NULL);
	labcomm_decoder_register_test_test_var(open_chan2_dec,
			handle_test_var_open2, NULL);

	labcomm_decoder_register_test_test_var(recv_chan1_dec,
			handle_test_var_recv1, NULL);
	labcomm_decoder_register_test_test_var(recv_chan2_dec,
			handle_test_var_recv2, NULL);

	// register types on encoders, it will spawn one event per registration
	labcomm_encoder_register_test_test_var(open_chan1_enc);
	firefly_event_execute(firefly_event_pop(eq));
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	free_tmp_data(&conn_open_write);
	firefly_event_execute(firefly_event_pop(eq));

	labcomm_encoder_register_test_test_var(open_chan2_enc);
	firefly_event_execute(firefly_event_pop(eq));
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	free_tmp_data(&conn_open_write);
	firefly_event_execute(firefly_event_pop(eq));

	labcomm_encoder_register_test_test_var(recv_chan1_enc);
	firefly_event_execute(firefly_event_pop(eq));
	protocol_data_received(conn_open, conn_recv_write.data,
			conn_recv_write.size);
	free_tmp_data(&conn_recv_write);
	firefly_event_execute(firefly_event_pop(eq));

	labcomm_encoder_register_test_test_var(recv_chan2_enc);
	firefly_event_execute(firefly_event_pop(eq));
	protocol_data_received(conn_open, conn_recv_write.data,
			conn_recv_write.size);
	free_tmp_data(&conn_recv_write);
	firefly_event_execute(firefly_event_pop(eq));

	// Send data on chan1 from conn_open
	test_test_var var_oc1 = 1;
	labcomm_encode_test_test_var(open_chan1_enc, &var_oc1);
	firefly_event_execute(firefly_event_pop(eq));
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	free_tmp_data(&conn_open_write);
	firefly_event_execute(firefly_event_pop(eq));
	CU_ASSERT_TRUE(recv_chan1_var_received);
	recv_chan1_var_received = false;
	// Send data on chan1 from conn_recv
	test_test_var var_oc2 = 2;
	labcomm_encode_test_test_var(open_chan2_enc, &var_oc2);
	firefly_event_execute(firefly_event_pop(eq));
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	free_tmp_data(&conn_open_write);
	firefly_event_execute(firefly_event_pop(eq));
	CU_ASSERT_TRUE(recv_chan2_var_received);
	recv_chan2_var_received = false;
	// Send data on chan2 from conn_open
	test_test_var var_rc1 = 3;
	labcomm_encode_test_test_var(recv_chan1_enc, &var_rc1);
	firefly_event_execute(firefly_event_pop(eq));
	protocol_data_received(conn_open, conn_recv_write.data,
			conn_recv_write.size);
	free_tmp_data(&conn_recv_write);
	firefly_event_execute(firefly_event_pop(eq));
	CU_ASSERT_TRUE(open_chan1_var_received);
	open_chan1_var_received = false;
	// Send data on chan2 from conn_recv
	test_test_var var_rc2 = 4;
	labcomm_encode_test_test_var(recv_chan2_enc, &var_rc2);
	firefly_event_execute(firefly_event_pop(eq));
	protocol_data_received(conn_open, conn_recv_write.data,
			conn_recv_write.size);
	free_tmp_data(&conn_recv_write);
	firefly_event_execute(firefly_event_pop(eq));
	CU_ASSERT_TRUE(open_chan2_var_received);
	open_chan2_var_received = false;

	// Close both channels from conn_open
	// save channel id's form testing later
	chan_id_conn_open = conn_open->chan_list->chan->local_id;
	chan_id_conn_recv = conn_open->chan_list->chan->remote_id;
	firefly_channel_close(conn_open->chan_list->chan);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	free_tmp_data(&conn_open_write);

	// Test only one channel remains
	CU_ASSERT_PTR_NULL(conn_open->chan_list->next);
	CU_ASSERT_PTR_NULL(conn_recv->chan_list->next);
	// Test the not closed channel remains
	CU_ASSERT_NOT_EQUAL(conn_recv->chan_list->chan->local_id,
			chan_id_conn_recv);
	CU_ASSERT_NOT_EQUAL(conn_open->chan_list->chan->local_id,
			chan_id_conn_open);

	// Close the remaining channel
	firefly_channel_close(conn_open->chan_list->chan);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	free_tmp_data(&conn_open_write);
	// Test no channel remains
	CU_ASSERT_PTR_NULL(conn_open->chan_list);
	CU_ASSERT_PTR_NULL(conn_recv->chan_list);

	// Clean up
	firefly_connection_free(&conn_open);
	firefly_connection_free(&conn_recv);
	firefly_event_queue_free(&eq);
}

void test_nbr_chan()
{
	struct firefly_connection conn;
	struct channel_list_node ln[2];

	conn.chan_list = NULL;
	CU_ASSERT_EQUAL(firefly_number_channels_in_connection(&conn), 0);

	ln[0].next = NULL;
	conn.chan_list = &ln[0];
	CU_ASSERT_EQUAL(firefly_number_channels_in_connection(&conn), 1);

	ln[1].next = NULL;
	conn.chan_list->next = &ln[1];
	CU_ASSERT_EQUAL(firefly_number_channels_in_connection(&conn), 2);
}
