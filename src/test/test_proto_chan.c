#include "CUnit/Basic.h"

#include <labcomm.h>
#include <labcomm_mem_writer.h>
#include <labcomm_mem_reader.h>
#include <labcomm_fd_reader_writer.h>

#include <stdbool.h>

#include <gen/firefly_protocol.h>
#include <gen/test.h>
#include <firefly_errors.h>
#include "protocol/firefly_protocol_private.h"
#include <protocol/firefly_protocol.h>
#include "eventqueue/event_queue.h"

#include "test/test_labcomm_utils.h"

#define REMOTE_CHAN_ID (2)	// Chan id used by all simulated remote channels.

static struct labcomm_decoder *test_dec;
static labcomm_mem_reader_context_t *test_dec_ctx;

struct encoded_packet {
	unsigned char *sign;
	unsigned char *data;
	size_t ssize;
	size_t dsize;
};

struct encoded_packet *create_encoded_packet(lc_register_f reg, lc_encode_f enc,
		void *data)
{
	struct encoded_packet *enc_pkt = malloc(sizeof(struct encoded_packet));
	create_labcomm_files_general( reg, enc, data);
	enc_pkt->ssize = read_file_to_mem(&enc_pkt->sign, SIG_FILE);
	enc_pkt->dsize = read_file_to_mem(&enc_pkt->data, DATA_FILE);
	return enc_pkt;
}

void encoded_packet_free(struct encoded_packet *ep)
{
	free(ep->data);
	free(ep->sign);
	free(ep);
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

void setup_test_decoder_new()
{
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
}

void teardown_test_decoder_free()
{
	labcomm_decoder_free(test_dec);
	test_dec = NULL;
	free(test_dec_ctx);
	test_dec_ctx = NULL;
}

struct firefly_connection *setup_test_conn_new(firefly_channel_is_open_f ch_op,
		firefly_channel_closed_f ch_cl, firefly_channel_accept_f ch_acc,
		struct firefly_event_queue *eq)
{
	struct firefly_connection *conn =
		firefly_connection_new(ch_op, ch_cl, ch_acc, eq);
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
	return conn;
}

int init_suit_proto_chan()
{
	return 0; // Success.
}

int clean_suit_proto_chan()
{
	return 0; // Success.
}

static bool sent_chan_req = false;
static bool sent_chan_ack = false;

static bool chan_opened_called = false;
void chan_opened_mock(struct firefly_channel *chan)
{
	chan_opened_called = true;
}

#define MAX_TESTED_ID (10)

void test_next_channel_id()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add);
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

void test_chan_open()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	// Init connection and register error handler on encoder and decoder
	struct firefly_connection *conn =
		setup_test_conn_new(chan_opened_mock, NULL, NULL, eq);

	// Init decoder to test data that is sent
	setup_test_decoder_new();
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
	firefly_channel_open(conn, NULL);

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
	struct encoded_packet *ep = create_encoded_packet(
		labcomm_encoder_register_firefly_protocol_channel_response,
		(lc_encode_f) labcomm_encode_firefly_protocol_channel_response,
		&chan_resp);
	protocol_data_received(conn, ep->sign, ep->ssize);
	protocol_data_received(conn, ep->data, ep->dsize);

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
	free(ep->sign);
	free(ep->data);
	free(ep);
	teardown_test_decoder_free();
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

static bool sent_response = false;
static bool chan_accept_called = false;

static bool chan_recv_accept_chan(struct firefly_channel *chan)
{
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

void test_chan_recv_accept()
{
	struct encoded_packet *ep;
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	// Init connection and register error handler on encoder and decoder
	struct firefly_connection *conn =
		setup_test_conn_new(chan_opened_mock, NULL,
				chan_recv_accept_chan, eq);

	// Init decoder to test data that is sent
	setup_test_decoder_new();
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
	ep = create_encoded_packet(
		labcomm_encoder_register_firefly_protocol_channel_request,
		(lc_encode_f) labcomm_encode_firefly_protocol_channel_request,
		&chan_req);
	// Give channel request data to protocol layer.
	protocol_data_received(conn, ep->sign, ep->ssize);
	protocol_data_received(conn, ep->data, ep->dsize);
	encoded_packet_free(ep);

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
	ep = create_encoded_packet(
		labcomm_encoder_register_firefly_protocol_channel_ack,
		(lc_encode_f) labcomm_encode_firefly_protocol_channel_ack,
		&ack);
	// Fixe type ID due to multiple types on proto_decoder
	ep->data[3] = 0x81;
	ep->sign[7] = 0x81;

	// Recieving end and got an ack
	protocol_data_received(conn, ep->sign, ep->ssize);
	protocol_data_received(conn, ep->data, ep->dsize);
	encoded_packet_free(ep);
	ev = firefly_event_pop(eq);
	firefly_event_execute(ev);

	CU_ASSERT_TRUE(chan_opened_called);

	chan_opened_called = false;
	teardown_test_decoder_free();
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

static void chan_recv_reject_handle_chan_res(
		firefly_protocol_channel_response *res, void *context)
{
	CU_ASSERT_FALSE(res->ack);
	CU_ASSERT_EQUAL(res->dest_chan_id, REMOTE_CHAN_ID);
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
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	// Setup connection
	struct firefly_connection *conn =
		setup_test_conn_new(chan_opened_mock, NULL,
				       chan_recv_reject_chan, eq);

	// Init decoder to test data that is sent
	setup_test_decoder_new();
	labcomm_decoder_register_firefly_protocol_channel_response(test_dec,
			chan_recv_reject_handle_chan_res, NULL);

	// register chan_req on conn decoder and chan_res on conn encoder
	labcomm_decoder_register_firefly_protocol_channel_request(
			conn->transport_decoder, handle_channel_request, conn);
	labcomm_encoder_register_firefly_protocol_channel_response(
			conn->transport_encoder);

	// create temporary labcomm encoder to send request.
	firefly_protocol_channel_request chan_req;
	chan_req.dest_chan_id = CHANNEL_ID_NOT_SET;
	chan_req.source_chan_id = REMOTE_CHAN_ID;
	struct encoded_packet *ep = create_encoded_packet(
			labcomm_encoder_register_firefly_protocol_channel_request,
			(lc_encode_f) labcomm_encode_firefly_protocol_channel_request,
			&chan_req);
	// send channel request
	protocol_data_received(conn, ep->sign, ep->ssize);
	protocol_data_received(conn, ep->data, ep->dsize);
	encoded_packet_free(ep);

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
	teardown_test_decoder_free();
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
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

static bool chan_rejected_called = false;
void chan_open_chan_rejected(struct firefly_connection *conn)
{
	chan_rejected_called = true;
}

void test_chan_open_rejected()
{

	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add);
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

	// Init decoder to test data that is sent
	setup_test_decoder_new();
	labcomm_decoder_register_firefly_protocol_channel_request(test_dec,
			chan_open_reject_handle_chan_req, NULL);
	labcomm_decoder_register_firefly_protocol_channel_ack(test_dec,
			chan_open_reject_handle_chan_ack, NULL);

	// register chan_req on conn decoder and chan_res on conn encoder
	labcomm_decoder_register_firefly_protocol_channel_response(
			conn->transport_decoder, handle_channel_response, conn);
	labcomm_encoder_register_firefly_protocol_channel_request(conn->transport_encoder);
	labcomm_encoder_register_firefly_protocol_channel_ack(conn->transport_encoder);

	firefly_channel_open(conn, chan_open_chan_rejected);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	CU_ASSERT_TRUE(sent_chan_req);

	firefly_protocol_channel_response chan_res;
	chan_res.dest_chan_id = conn->chan_list->chan->local_id;
	chan_res.source_chan_id = CHANNEL_ID_NOT_SET;
	chan_res.ack = false;

	struct encoded_packet *ep = create_encoded_packet(
			labcomm_encoder_register_firefly_protocol_channel_response,
			(lc_encode_f) labcomm_encode_firefly_protocol_channel_response,
			&chan_res);
	// send response
	protocol_data_received(conn, ep->sign, ep->ssize);
	protocol_data_received(conn, ep->data, ep->dsize);
	encoded_packet_free(ep);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	CU_ASSERT_FALSE(sent_chan_ack);
	CU_ASSERT_PTR_NULL(conn->chan_list);
	// test resources are destroyed again
	CU_ASSERT_FALSE(chan_opened_called);
	CU_ASSERT_TRUE(chan_rejected_called);

	// Clean up
	chan_rejected_called = false;
	sent_chan_ack = false;
	sent_chan_req = false;
	chan_opened_called = false;
	teardown_test_decoder_free();
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

static bool conn_recv_accept_called = false;

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
	struct firefly_connection *conn_open;
	struct firefly_connection *conn_recv;

	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add);
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

	// Clean up
	chan_opened_called = false;
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
}

static bool chan_closed_called = false;
void chan_closed_cb(struct firefly_channel *chan)
{
	chan_closed_called = true;
}

void test_chan_close()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add);
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

	// Init decoder to test data that is sent
	setup_test_decoder_new();
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
	CU_ASSERT_TRUE(chan_closed_called);

	// clean up
	chan_closed_called = false;
	teardown_test_decoder_free();
	firefly_connection_free(&conn);
	firefly_event_queue_free(&eq);
}

void test_chan_recv_close()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}

	// Setup connection
	struct firefly_connection *conn =
		setup_test_conn_new(NULL, chan_closed_cb, NULL, eq);
	labcomm_decoder_register_firefly_protocol_channel_close(
			conn->transport_decoder, handle_channel_close, conn);

	// create channel
	struct firefly_channel *chan = firefly_channel_new(conn);
	chan->remote_id = REMOTE_CHAN_ID;
	add_channel_to_connection(chan, conn);

	// create close channel packet
	firefly_protocol_channel_close chan_close;
	chan_close.dest_chan_id = conn->chan_list->chan->local_id;
	chan_close.source_chan_id = REMOTE_CHAN_ID;

	struct encoded_packet *ep = create_encoded_packet(
			labcomm_encoder_register_firefly_protocol_channel_close,
			(lc_encode_f) labcomm_encode_firefly_protocol_channel_close,
			&chan_close);

	// give packet to protocol
	protocol_data_received(conn, ep->sign, ep->ssize);
	protocol_data_received(conn, ep->data, ep->dsize);
	encoded_packet_free(ep);
	// pop and execute event
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ev);
	CU_ASSERT_EQUAL(ev->base.type, EVENT_CHAN_CLOSE);
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

static bool sent_app_sample = false;
static bool sent_app_data = false;

struct encoded_packet app_data;
void handle_data_sample(firefly_protocol_data_sample *data, void *ctx)
{
	struct firefly_channel *ch = (struct firefly_channel *) ctx;
	CU_ASSERT_EQUAL(ch->local_id, data->src_chan_id);
	CU_ASSERT_EQUAL(ch->remote_id, data->dest_chan_id);
	sent_app_sample = true;
	app_data.sign = malloc(data->app_enc_data.n_0);
	app_data.ssize = data->app_enc_data.n_0;
	memcpy(app_data.sign, data->app_enc_data.a, data->app_enc_data.n_0);
}

void handle_test_test_var(test_test_var *data, void *ctx)
{
	CU_ASSERT_EQUAL(*data, 1);
	sent_app_data = true;
}

void test_send_app_data()
{
	test_test_var app_test_data = 1;
	struct firefly_event *ev;

	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}

	setup_test_decoder_new();
	// Setup connection
	struct firefly_connection *conn =
		setup_test_conn_new(NULL, chan_closed_cb, NULL, eq);

	struct firefly_channel *ch = firefly_channel_new(conn);
	ch->remote_id = REMOTE_CHAN_ID;
	add_channel_to_connection(ch, conn);

	labcomm_decoder_register_firefly_protocol_data_sample(test_dec,
			handle_data_sample, ch);

	struct labcomm_mem_reader_context_t *test_dec_ctx_2;
	test_dec_ctx_2 = malloc(sizeof(labcomm_mem_reader_context_t));
	if (test_dec_ctx_2 == NULL) {
		CU_FAIL("Test decoder context was null\n");
	}
	test_dec_ctx_2->enc_data = NULL;
	test_dec_ctx_2->size = 0;
	struct labcomm_decoder *test_dec_2 = labcomm_decoder_new(labcomm_mem_reader,
			test_dec_ctx_2);
	labcomm_register_error_handler_decoder(test_dec_2, handle_labcomm_error);
	labcomm_decoder_register_test_test_var(test_dec_2, handle_test_test_var, NULL);

	labcomm_encoder_register_firefly_protocol_data_sample(
			conn->transport_encoder);
	struct labcomm_encoder *ch_enc = firefly_protocol_get_output_stream(ch);
	labcomm_encoder_register_test_test_var(ch_enc);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	CU_ASSERT_TRUE(sent_app_sample);
	sent_app_sample = false;

	conn->writer_data->pos = 0;

	test_dec_ctx_2->enc_data = app_data.sign;
	test_dec_ctx_2->size = app_data.ssize;
	labcomm_decoder_decode_one(test_dec_2);
	test_dec_ctx_2->enc_data = NULL;
	test_dec_ctx_2->size = 0;
	free(app_data.sign);
	app_data.sign = NULL;
	// decode _2

	labcomm_encode_test_test_var(ch_enc, &app_test_data);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	CU_ASSERT_TRUE(sent_app_sample);

	test_dec_ctx_2->enc_data = app_data.sign;
	test_dec_ctx_2->size = app_data.ssize;
	labcomm_decoder_decode_one(test_dec_2);
	test_dec_ctx_2->enc_data = NULL;
	test_dec_ctx_2->size = 0;
	// decode _2
	free(app_data.sign);
	app_data.sign = NULL;

	CU_ASSERT_TRUE(sent_app_data);
	sent_app_sample = false;
	sent_app_data = false;
	firefly_connection_free(&conn);
	teardown_test_decoder_free();
	labcomm_decoder_free(test_dec_2);
	free(test_dec_ctx_2);
	firefly_event_queue_free(&eq);
}
