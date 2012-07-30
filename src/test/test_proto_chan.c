#include "CUnit/Basic.h"

#include <labcomm.h>
#include <labcomm_mem_writer.h>
#include <labcomm_mem_reader.h>
#include <labcomm_fd_reader_writer.h>

#include <stdbool.h>

#include <gen/firefly_protocol.h>
#include <firefly_errors.h>
#include "protocol/firefly_protocol_private.h"
#include <protocol/firefly_protocol.h>
#include "eventqueue/event_queue.h"

#include "test/test_labcomm_utils.h"

#define REMOTE_CHAN_ID (2)	// Chan id used by all simulated remote channels.

static struct labcomm_decoder *test_dec;
static labcomm_mem_reader_context_t *test_dec_ctx;
static bool sent_chan_req = false;
static bool sent_chan_ack = false;

int init_suit_proto_chan()
{
	return 0; // Success.
}

int clean_suit_proto_chan()
{
	return 0; // Success.
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

static void chan_open_handle_ack(firefly_protocol_channel_ack *ack, void *ctx)
{
	struct firefly_channel *chan = ((struct firefly_connection *) ctx)->
		chan_list->chan;
	CU_ASSERT_EQUAL(chan->local_id, ack->source_chan_id);
	CU_ASSERT_EQUAL(chan->remote_id, ack->dest_chan_id);
	CU_ASSERT_EQUAL(chan->remote_id, REMOTE_CHAN_ID);
	CU_ASSERT_TRUE(ack->ack);
	sent_chan_ack = true;
}
static void chan_open_handle_req(firefly_protocol_channel_request *req,
		void *ctx)
{
	struct firefly_channel *chan = ((struct firefly_connection *) ctx)->
		chan_list->chan;
	CU_ASSERT_EQUAL(chan->local_id, req->source_chan_id);
	CU_ASSERT_EQUAL(CHANNEL_ID_NOT_SET, req->dest_chan_id);
	sent_chan_req = true;
}
static bool chan_opened_called = false;
void chan_open_is_open_mock(struct firefly_channel *chan)
{
	chan_opened_called = true;
}

void chan_open_transport_write_mock(unsigned char *data, size_t data_size,
		struct firefly_connection *conn)
{
	test_dec_ctx->enc_data = data;
	test_dec_ctx->size = data_size;
	labcomm_decoder_decode_one(test_dec);
	test_dec_ctx->enc_data = NULL;
	test_dec_ctx->size = 0;
}

void test_chan_open()
{
	struct firefly_event_queue *eq;

	eq = firefly_event_queue_new();
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	eq->offer_event_cb = firefly_event_add;
	// Init connection and register error handler on encoder and decoder
	struct firefly_connection *conn =
		firefly_connection_new(chan_open_is_open_mock, NULL, eq);
	if (conn == NULL) {
		CU_FAIL("Could not create connection.\n");
	}
	// Set some stuff that firefly_init_conn doesn't do
	conn->transport_conn_platspec = NULL;
	conn->transport_write = chan_open_transport_write_mock;
	labcomm_register_error_handler_encoder(conn->transport_encoder,
			handle_labcomm_error);
	labcomm_register_error_handler_decoder(conn->transport_decoder,
			handle_labcomm_error);

	// Init decoder to test data that is sent
	test_dec_ctx = malloc(sizeof(labcomm_mem_reader_context_t));
	if (test_dec_ctx == NULL) {
		CU_FAIL("Test decoder context was null\n");
	}
	test_dec_ctx->enc_data = NULL;
	test_dec_ctx->size = 0;
	test_dec = labcomm_decoder_new(labcomm_mem_reader, test_dec_ctx);
	if (test_dec == NULL) {
		CU_FAIL("Test decoder was null\n");
	}
	labcomm_register_error_handler_decoder(test_dec, handle_labcomm_error);
	labcomm_decoder_register_firefly_protocol_channel_request(test_dec,
			chan_open_handle_req, conn);
	labcomm_decoder_register_firefly_protocol_channel_ack(test_dec,
			chan_open_handle_ack, conn);

	// Register channel_request on transport encoder
	labcomm_encoder_register_firefly_protocol_channel_request(
			conn->transport_encoder);
	conn->writer_data->pos = 0;

	// Register channel_ack on transport encoder
	labcomm_encoder_register_firefly_protocol_channel_ack(
			conn->transport_encoder);
	conn->writer_data->pos = 0;

	// Register channel response on transport decoder
	labcomm_decoder_register_firefly_protocol_channel_response(
			conn->transport_decoder, handle_channel_response, conn);

	// Open a new channel
	firefly_channel_open(conn);

	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	// Test that a chan open request is sent.
	CU_ASSERT_TRUE(sent_chan_req);
	sent_chan_req = false;

	// Simulate that we received a channel response from the other end
	firefly_protocol_channel_response chan_resp;
	chan_resp.source_chan_id = REMOTE_CHAN_ID;
	chan_resp.dest_chan_id = conn->chan_list->chan->local_id;
	chan_resp.ack = true;
	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_channel_response,
		(lc_encode_f) labcomm_encode_firefly_protocol_channel_response,
		&chan_resp);
	unsigned char *sign;
	unsigned char *data;
	size_t sign_size = read_file_to_mem(&sign, SIG_FILE);
	size_t data_size = read_file_to_mem(&data, DATA_FILE);
	protocol_data_received(conn, sign, sign_size);
	protocol_data_received(conn, data, data_size);

	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	// Test that we sent an ack
	CU_ASSERT_TRUE(sent_chan_ack);
	sent_chan_ack = false;

	// test chan_is_open is called
	CU_ASSERT_TRUE(chan_opened_called);

	// Clean up
	chan_opened_called = false;
	free(sign);
	free(data);
	labcomm_decoder_free(test_dec);
	test_dec = NULL;
	free(test_dec_ctx);
	test_dec_ctx = NULL;
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

static void chan_recv_chan_opened_mock(struct firefly_channel *chan)
{
	chan_opened_called = true;
}

static bool sent_response = false;
static bool chan_accept_called = false;
static struct firefly_channel *accepted_chan;
static bool chan_recv_accept_chan(struct firefly_channel *chan)
{
	accepted_chan = chan;
	CU_ASSERT_EQUAL(chan->remote_id, REMOTE_CHAN_ID);
	chan_accept_called = true;
	return true;
}

void chan_recv_handle_res(firefly_protocol_channel_response *res, void *ctx)
{
	struct firefly_connection *conn = (struct firefly_connection *) ctx;
	struct firefly_channel *chan = conn->chan_list->chan;
	CU_ASSERT_EQUAL(res->dest_chan_id, REMOTE_CHAN_ID);
	CU_ASSERT_EQUAL(chan->remote_id, REMOTE_CHAN_ID);
	CU_ASSERT_EQUAL(chan->local_id, res->source_chan_id);
	sent_response = true;
}

void transport_write_test_decoder(unsigned char *data, size_t size,
		struct firefly_connection *conn)
{
	test_dec_ctx->enc_data = data;
	test_dec_ctx->size = size;
	labcomm_decoder_decode_one(test_dec);
	test_dec_ctx->enc_data = NULL;
	test_dec_ctx->size = 0;
}

void chan_recv_test_resp(firefly_protocol_channel_response *res, void *context)
{
	CU_ASSERT_EQUAL(res->dest_chan_id, 1); // TODO 1?
	CU_ASSERT_EQUAL(res->source_chan_id, accepted_chan->local_id);
	CU_ASSERT_TRUE(res->ack);
}

void test_chan_recv_accept()
{
	struct firefly_event_queue *eq;

	eq = firefly_event_queue_new();
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	eq->offer_event_cb = firefly_event_add;
	// Init connection and register error handler on encoder and decoder
	struct firefly_connection *conn =
		firefly_connection_new(chan_recv_chan_opened_mock,
				chan_recv_accept_chan, eq);
	if (conn == NULL) {
		CU_FAIL("Could not create connection.\n");
	}
	// Set some stuff that firefly_init_conn doesn't do
	conn->transport_conn_platspec = NULL;
	conn->transport_write = transport_write_test_decoder;
	labcomm_register_error_handler_encoder(conn->transport_encoder,
			handle_labcomm_error);
	labcomm_register_error_handler_decoder(conn->transport_decoder,
			handle_labcomm_error);

	// Init decoder to test data that is sent
	test_dec_ctx = malloc(sizeof(labcomm_mem_reader_context_t));
	test_dec_ctx->enc_data = NULL;
	test_dec_ctx->size = 0;
	test_dec = labcomm_decoder_new(labcomm_mem_reader, test_dec_ctx);
	if (test_dec == NULL) {
		CU_FAIL("Encoder was null\n");
	}
	labcomm_register_error_handler_decoder(test_dec, handle_labcomm_error);
	labcomm_decoder_register_firefly_protocol_channel_response(test_dec,
			chan_recv_handle_res, conn);

	// Register all LabComm types used on this connection.
	labcomm_decoder_register_firefly_protocol_channel_request(
			conn->transport_decoder, handle_channel_request, conn);
	labcomm_decoder_register_firefly_protocol_channel_ack(
			conn->transport_decoder, handle_channel_ack, conn);
	labcomm_encoder_register_firefly_protocol_channel_response(
						conn->transport_encoder);
	// Create channel request.
	firefly_protocol_channel_request chan_req;
	chan_req.source_chan_id = REMOTE_CHAN_ID;
	chan_req.dest_chan_id = CHANNEL_ID_NOT_SET;
	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_channel_request,
		(lc_encode_f) labcomm_encode_firefly_protocol_channel_request,
		&chan_req);
	unsigned char *sign;
	unsigned char *data;
	size_t sign_size = read_file_to_mem(&sign, SIG_FILE);
	size_t data_size = read_file_to_mem(&data, DATA_FILE);

	// Give channel request data to protocol layer.
	protocol_data_received(conn, sign, sign_size);
	protocol_data_received(conn, data, data_size);

	struct firefly_event *ev = firefly_event_pop(eq);
	firefly_event_execute(ev);

	// Test accept called.
	CU_ASSERT_TRUE(chan_accept_called);
	chan_accept_called = false; // Reset value.

	CU_ASSERT_TRUE(sent_response);
	sent_response = false;

	// simulate an ACK is sent from remote
	firefly_protocol_channel_ack ack;
	ack.dest_chan_id = conn->chan_list->chan->local_id;
	ack.source_chan_id = conn->chan_list->chan->remote_id;
	ack.ack = true;
	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_channel_ack,
		(lc_encode_f) labcomm_encode_firefly_protocol_channel_ack,
		&ack);

	unsigned char *ack_sign;
	unsigned char *ack_data;
	size_t ack_sign_size = read_file_to_mem(&ack_sign, SIG_FILE);
	size_t ack_data_size = read_file_to_mem(&ack_data, DATA_FILE);
	// Fixe type ID due to multiple types on proto_decoder
	ack_data[3] = 0x81;
	ack_sign[7] = 0x81;

	// Recieving end and got an ack
	protocol_data_received(conn, ack_sign, ack_sign_size);
	protocol_data_received(conn, ack_data, ack_data_size);
	ev = firefly_event_pop(eq);
	firefly_event_execute(ev);

	CU_ASSERT_TRUE(chan_opened_called);
	chan_opened_called = false;
	free(sign);
	free(data);
	free(ack_sign);
	free(ack_data);
	labcomm_decoder_free(test_dec);
	free(test_dec_ctx);
	firefly_connection_free(&conn);
}

static void chan_recv_reject_handle_chan_res(
		firefly_protocol_channel_response *res, void *context)
{
	CU_ASSERT_FALSE(res->ack);
	CU_ASSERT_EQUAL(res->dest_chan_id, REMOTE_CHAN_ID);
	// TODO should remote chan be any specific value? seams not nessesary.
	CU_ASSERT_EQUAL(res->source_chan_id, CHANNEL_ID_NOT_SET);
	sent_response = true;
}

static bool chan_recv_reject_chan(struct firefly_channel *chan)
{
	chan_accept_called = true;
	return false;
}

void test_chan_recv_reject()
{
	struct firefly_event_queue *eq;

	eq = firefly_event_queue_new();
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	eq->offer_event_cb = firefly_event_add;
	// Setup connection
	struct firefly_connection *conn =
		firefly_connection_new(chan_recv_chan_opened_mock,
				       chan_recv_reject_chan, eq);
	if (conn == NULL) {
		CU_FAIL("Could not create connection.\n");
	}
	// Set some stuff that firefly_init_conn doesn't do
	conn->transport_conn_platspec = NULL;
	conn->transport_write = transport_write_test_decoder;
	labcomm_register_error_handler_encoder(conn->transport_encoder,
			handle_labcomm_error);
	labcomm_register_error_handler_decoder(conn->transport_decoder,
			handle_labcomm_error);

	// Init decoder to test data that is sent
	test_dec_ctx = malloc(sizeof(labcomm_mem_reader_context_t));
	test_dec_ctx->enc_data = NULL;
	test_dec_ctx->size = 0;
	test_dec = labcomm_decoder_new(labcomm_mem_reader, test_dec_ctx);
	if (test_dec == NULL) {
		CU_FAIL("Encoder was null\n");
	}
	labcomm_register_error_handler_decoder(test_dec, handle_labcomm_error);
	labcomm_decoder_register_firefly_protocol_channel_response(test_dec,
			chan_recv_reject_handle_chan_res, NULL);

	// register chan_req on conn decoder and chan_res on conn encoder
	labcomm_decoder_register_firefly_protocol_channel_request(conn->transport_decoder, handle_channel_request, conn);
	labcomm_encoder_register_firefly_protocol_channel_response(conn->transport_encoder);

	// create temporary labcomm encoder to send request.
	firefly_protocol_channel_request chan_req;
	chan_req.dest_chan_id = CHANNEL_ID_NOT_SET;
	chan_req.source_chan_id = REMOTE_CHAN_ID;
	create_labcomm_files_general(labcomm_encoder_register_firefly_protocol_channel_request,
			(lc_encode_f) labcomm_encode_firefly_protocol_channel_request,
			&chan_req);
	unsigned char *chan_req_sig;
	unsigned char *chan_req_data;
	size_t chan_req_ssize = read_file_to_mem(&chan_req_sig, SIG_FILE);
	size_t chan_req_dsize = read_file_to_mem(&chan_req_data, DATA_FILE);

	// send channel request
	protocol_data_received(conn, chan_req_sig, chan_req_ssize);
	protocol_data_received(conn, chan_req_data, chan_req_dsize);

	struct firefly_event *ev = firefly_event_pop(eq);
	firefly_event_execute(ev);

	// check accept called
	CU_ASSERT_TRUE(chan_accept_called);
	// check chan_res sent and correctly set
	CU_ASSERT_TRUE(sent_response);

	// check chan_opened is not called.
	CU_ASSERT_FALSE(chan_opened_called);

	// check channel is destroyed and all related resources dealloced
	CU_ASSERT_PTR_NULL(conn->chan_list);

	// Clean up
	chan_opened_called = false;
	chan_accept_called = false;
	sent_response = false;
	free(chan_req_sig);
	free(chan_req_data);
	labcomm_decoder_free(test_dec);
	free(test_dec_ctx);
	firefly_connection_free(&conn);
}

void chan_open_reject_handle_chan_req(firefly_protocol_channel_request *req,
		void *context)
{
	sent_chan_req = true;
}

void chan_open_reject_handle_chan_ack(firefly_protocol_channel_ack *ack,
		void *context)
{
	sent_chan_ack = true;
}

void test_chan_open_rejected()
{

  	struct firefly_event_queue *eq;

	eq = firefly_event_queue_new();
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	eq->offer_event_cb = firefly_event_add;

	// Setup connection
	struct firefly_connection *conn =
		firefly_connection_new(chan_recv_chan_opened_mock,
				       NULL, eq);
	if (conn == NULL) {
		CU_FAIL("Could not create connection.\n");
	}
	// Set some stuff that firefly_init_conn doesn't do
	conn->transport_conn_platspec = NULL;
	conn->transport_write = transport_write_test_decoder;
	labcomm_register_error_handler_encoder(conn->transport_encoder,
			handle_labcomm_error);
	labcomm_register_error_handler_decoder(conn->transport_decoder,
			handle_labcomm_error);

	// Init decoder to test data that is sent
	test_dec_ctx = malloc(sizeof(labcomm_mem_reader_context_t));
	test_dec_ctx->enc_data = NULL;
	test_dec_ctx->size = 0;
	test_dec = labcomm_decoder_new(labcomm_mem_reader, test_dec_ctx);
	if (test_dec == NULL) {
		CU_FAIL("Encoder was null\n");
	}
	labcomm_register_error_handler_decoder(test_dec, handle_labcomm_error);
	labcomm_decoder_register_firefly_protocol_channel_request(test_dec,
			chan_open_reject_handle_chan_req, NULL);
	labcomm_decoder_register_firefly_protocol_channel_ack(test_dec,
			chan_open_reject_handle_chan_ack, NULL);

	// register chan_req on conn decoder and chan_res on conn encoder
	labcomm_decoder_register_firefly_protocol_channel_response(
			conn->transport_decoder, handle_channel_response, conn);
	labcomm_encoder_register_firefly_protocol_channel_request(conn->transport_encoder);
	labcomm_encoder_register_firefly_protocol_channel_ack(conn->transport_encoder);

	firefly_channel_open(conn);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	CU_ASSERT_TRUE(sent_chan_req);

	firefly_protocol_channel_response chan_res;
	chan_res.dest_chan_id = conn->chan_list->chan->local_id;
	chan_res.source_chan_id = CHANNEL_ID_NOT_SET;
	chan_res.ack = false;

	create_labcomm_files_general(labcomm_encoder_register_firefly_protocol_channel_response,
			(lc_encode_f) labcomm_encode_firefly_protocol_channel_response,
			&chan_res);
	unsigned char *chan_res_sig;
	unsigned char *chan_res_data;
	size_t chan_res_ssize = read_file_to_mem(&chan_res_sig, SIG_FILE);
	size_t chan_res_dsize = read_file_to_mem(&chan_res_data, DATA_FILE);
	// send response
	protocol_data_received(conn, chan_res_sig, chan_res_ssize);
	protocol_data_received(conn, chan_res_data, chan_res_dsize);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	CU_ASSERT_FALSE(sent_chan_ack);
	CU_ASSERT_PTR_NULL(conn->chan_list);
	// test resources are destroyed again
	CU_ASSERT_FALSE(chan_opened_called);

	// Clean up
	sent_chan_ack = false;
	sent_chan_req = false;
	chan_opened_called = false;
	labcomm_decoder_free(test_dec);
	free(test_dec_ctx);
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
	free(chan_res_sig);
	free(chan_res_data);
}

static struct firefly_connection *conn_open;
static struct firefly_connection *conn_recv;
static bool conn_open_chan_opened = false;
static bool conn_recv_chan_opened = false;
static bool conn_recv_accept_called = false;

void chan_open_recv_chan_open_open(struct firefly_channel *chan)
{
	conn_open_chan_opened = true;
}

void chan_open_recv_chan_open_recv(struct firefly_channel *chan)
{
	conn_recv_chan_opened = true;
}

bool chan_open_recv_accept_open(struct firefly_channel *chan)
{
	CU_FAIL("Connection open received channel.");
	return false;
}

bool chan_open_recv_accept_recv(struct firefly_channel *chan)
{
	return conn_recv_accept_called = true;
}

struct tmp_data {
	unsigned char *data;
	size_t size;
};

static struct tmp_data conn_open_write;
static struct tmp_data conn_recv_write;

void free_tmp_data(struct tmp_data *td)
{
	free(td->data);
	td->data = NULL;
	td->size = 0;
}

void chan_open_recv_write_open(unsigned char *data, size_t size,
		struct firefly_connection *conn)
{
	conn_open_write.data = malloc(size);
	memcpy(conn_open_write.data, data, size);
	conn_open_write.size = size;
}

void chan_open_recv_write_recv(unsigned char *data, size_t size,
		struct firefly_connection *conn)
{
	conn_recv_write.data = malloc(size);
	memcpy(conn_recv_write.data, data, size);
	conn_recv_write.size = size;
}

void test_chan_open_recv()
{
	struct firefly_event_queue *eq;

	eq = firefly_event_queue_new();
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	eq->offer_event_cb = firefly_event_add;
	// Init connection to open channel and register error handler on encoder
	// and decoder
	conn_open = firefly_connection_new(chan_open_recv_chan_open_open,
									   chan_open_recv_accept_open, eq);
	if (conn_open == NULL) {
		CU_FAIL("Could not create connection.\n");
	}
	// Set some stuff that firefly_init_conn doesn't do
	conn_open->transport_conn_platspec = NULL;
	conn_open->transport_write = chan_open_recv_write_open;
	labcomm_register_error_handler_encoder(conn_open->transport_encoder,
			handle_labcomm_error);
	labcomm_register_error_handler_decoder(conn_open->transport_decoder,
			handle_labcomm_error);

	// Init connection to receive channel and register error handler on encoder
	// and decoder
	conn_recv = firefly_connection_new(chan_open_recv_chan_open_recv,
					chan_open_recv_accept_recv, eq);
	if (conn_recv == NULL) {
		CU_FAIL("Could not create connection.\n");
	}
	// Set some stuff that firefly_init_conn doesn't do
	conn_recv->transport_conn_platspec = NULL;
	conn_recv->transport_write = chan_open_recv_write_recv;
	labcomm_register_error_handler_encoder(conn_recv->transport_encoder,
			handle_labcomm_error);
	labcomm_register_error_handler_decoder(conn_recv->transport_decoder,
			handle_labcomm_error);

	// register decoders
	labcomm_decoder_register_firefly_protocol_channel_response(
			conn_open->transport_decoder, handle_channel_response, conn_open);
	labcomm_decoder_register_firefly_protocol_channel_request(
			conn_recv->transport_decoder, handle_channel_request, conn_recv);
	labcomm_decoder_register_firefly_protocol_channel_ack(
			conn_recv->transport_decoder, handle_channel_ack, conn_recv);

	// register encoders Signatures will be sent to each connection
	labcomm_encoder_register_firefly_protocol_channel_ack(
			conn_open->transport_encoder);
	conn_open->writer_data->pos = 0;
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	free_tmp_data(&conn_open_write);

	labcomm_encoder_register_firefly_protocol_channel_request(
			conn_open->transport_encoder);
	conn_open->writer_data->pos = 0;
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	free_tmp_data(&conn_open_write);

	labcomm_encoder_register_firefly_protocol_channel_response(
			conn_recv->transport_encoder);
	conn_recv->writer_data->pos = 0;
	protocol_data_received(conn_open, conn_recv_write.data,
			conn_recv_write.size);
	free_tmp_data(&conn_recv_write);

	// Init open channel from conn_open
	firefly_channel_open(conn_open);
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

	free_tmp_data(&conn_recv_write);
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	free_tmp_data(&conn_open_write);
	ev = firefly_event_pop(eq);
	firefly_event_execute(ev);

	CU_ASSERT_TRUE(conn_open_chan_opened);
	CU_ASSERT_TRUE(conn_recv_chan_opened);

	// Clean up
	firefly_connection_free(&conn_open);
	firefly_connection_free(&conn_recv);
	firefly_event_queue_free(&eq);
}

static bool chan_close_chan_closed = false;
void chan_close_chan_close(firefly_protocol_channel_close *chan_close, void *ctx)
{
	struct firefly_channel *chan = (struct firefly_channel *) ctx;
	CU_ASSERT_EQUAL(chan_close->dest_chan_id, chan->remote_id);
	CU_ASSERT_EQUAL(chan_close->source_chan_id, chan->local_id);
	chan_close_chan_closed = true;

	// assert close packet data
}

void test_chan_close()
{
	struct firefly_event_queue *eq;

	eq = firefly_event_queue_new();
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	eq->offer_event_cb = firefly_event_add;

	// Setup connection
	struct firefly_connection *conn =
		firefly_connection_new(NULL, NULL, eq);
	if (conn == NULL) {
		CU_FAIL("Could not create connection.\n");
	}
	// Set some stuff that firefly_init_conn doesn't do
	conn->transport_conn_platspec = NULL;
	conn->transport_write = transport_write_test_decoder;
	labcomm_register_error_handler_encoder(conn->transport_encoder,
			handle_labcomm_error);
	labcomm_register_error_handler_decoder(conn->transport_decoder,
			handle_labcomm_error);

	// create channel
	struct firefly_channel *chan = firefly_channel_new(conn);
	chan->remote_id = REMOTE_CHAN_ID;
	add_channel_to_connection(chan, conn);

	// Init decoder to test data that is sent
	test_dec_ctx = malloc(sizeof(labcomm_mem_reader_context_t));
	test_dec_ctx->enc_data = NULL;
	test_dec_ctx->size = 0;
	test_dec = labcomm_decoder_new(labcomm_mem_reader, test_dec_ctx);
	if (test_dec == NULL) {
		CU_FAIL("Encoder was null\n");
	}
	labcomm_register_error_handler_decoder(test_dec, handle_labcomm_error);
	// register close packet on test_dec
	labcomm_decoder_register_firefly_protocol_channel_close(test_dec,
			chan_close_chan_close, chan);

	// register close packet on conn encoder
	labcomm_encoder_register_firefly_protocol_channel_close(
			conn->transport_encoder);


	// close channel
	firefly_channel_close(chan, conn);
	// pop event queue and execute
		struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	CU_ASSERT_EQUAL(ev->base.type, EVENT_CHAN_CLOSE);
	firefly_event_execute(ev);

	// test close packet sent
	CU_ASSERT_TRUE(chan_close_chan_closed);
	// test channel state, free'd
	CU_ASSERT_PTR_NULL(conn->chan_list);

	// clean up
	labcomm_decoder_free(test_dec);
	free(test_dec_ctx);
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

void test_chan_recv_close()
{
	CU_FAIL("NOT IMPLEMENTED YET.");
}
