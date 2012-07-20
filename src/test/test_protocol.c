/**
 * @file
 * @brief Test the functionality of the protocol layer.
 */
#ifdef _GNU_SOURCE
#error "Something turned it on!"
#undef _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE (200112L)
#include <unistd.h>

#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include <fcntl.h>
#include <labcomm.h>
#include <labcomm_mem_writer.h>
#include <labcomm_mem_reader.h>
#include <labcomm_fd_reader_writer.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <gen/firefly_protocol.h>
#include <firefly_errors.h>
#include "protocol/firefly_protocol_private.h"
#include <protocol/firefly_protocol.h>

#define WRITE_BUF_SIZE (128)	// Size of the LabComm buffer to write to.
#define DATA_FILE ("data.enc")	// File where the encoded data can be written.
#define SIG_FILE ("sig.enc")	// File where the encoded data can be written.
#define REMOTE_CHAN_ID (2)	// Chan id used by all simulated remote channels.

int init_suit()
{
	return 0; // Success.
}

int clean_suit()
{
	return 0; // Success.
}

static unsigned int chan_id = 3;
static bool important = true;
static unsigned char app_data[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
static bool successfully_decoded = false;

typedef void (* lc_register_f)(struct labcomm_encoder *enc);
typedef void (* lc_encode_f)(struct labcomm_encoder *enc, void *data);
void create_labcomm_files_general(lc_register_f reg, lc_encode_f enc, void *data)
{
	int tmpfd = open(SIG_FILE, O_RDWR|O_CREAT|O_TRUNC, 0644);
	struct labcomm_encoder *fd_encoder =
			labcomm_encoder_new(labcomm_fd_writer, &tmpfd);
	reg(fd_encoder);
	close(tmpfd);
	tmpfd = open(DATA_FILE, O_RDWR|O_CREAT|O_TRUNC, 0644);
	enc(fd_encoder, data);
	close(tmpfd);
	labcomm_encoder_free(fd_encoder);
}

size_t read_file_to_mem(unsigned char **data, char *file_name)
{
	FILE *file = fopen(file_name, "rb");
	if (file == NULL) {
		CU_FAIL("Could not open file.\n");
	}

	int res = fseek(file, 0, SEEK_END);
	if (res != 0) {
		CU_FAIL("Could not seek file.\n");
	}
	long file_len = ftell(file);
	if (file_len == -1L) {
		perror("Ftell fcked.\n");
	}
	res = fseek(file, 0L, SEEK_SET);
	if (res != 0) {
		CU_FAIL("Could not seek file to begin.\n");
	}

	*data = calloc(1, file_len);
	if (*data == NULL) {
		CU_FAIL("Could not alloc filebuf.\n");
	}

	size_t units_read = fread(*data, file_len, 1, file);
	if (units_read != 1) {
		CU_FAIL("Did not read the whole file.\n");
	}

	fclose(file);
	return file_len;
}

void handle_labcomm_error(enum labcomm_error error_id, size_t nbr_va_args, ...)
{
	const char *err_msg = labcomm_error_get_str(error_id);
	if (err_msg == NULL) {
		err_msg = "Error with an unknown error ID occured.";
	}
	firefly_error(FIREFLY_ERROR_LABCOMM, 1, err_msg);

	CU_FAIL_FATAL("Labcomm error occured!\n");
}

void handle_firefly_protocol_data_sample(firefly_protocol_data_sample *proto,
		void *context)
{
	CU_ASSERT_EQUAL(proto->chan_id, chan_id);
	CU_ASSERT_EQUAL(proto->important, important);
	CU_ASSERT_EQUAL(proto->app_enc_data.n_0, sizeof(app_data));
	CU_ASSERT_EQUAL(0, memcmp(proto->app_enc_data.a, app_data,
						sizeof(app_data)));
	successfully_decoded = true;
}

// Simulate that the data is sent over the net an arrives the same way
// on the other end. Now we decoce it and hope it's the same stuff we
// previously encoded.
void transport_write_udp_posix_mock(unsigned char *data, size_t data_size,
		struct firefly_connection *conn)
{
	unsigned char *zero_buf = calloc(1, WRITE_BUF_SIZE);
	// The buffer shoule not be empty anymore.
	CU_ASSERT_NOT_EQUAL(0, memcmp(data, zero_buf, WRITE_BUF_SIZE));
	free(zero_buf);

}

void test_encode_decode_protocol()
{
	firefly_protocol_data_sample proto;
	proto.chan_id = chan_id;
	proto.important = important;
	proto.app_enc_data.n_0 = sizeof(app_data);
	proto.app_enc_data.a = app_data;

	struct ff_transport_data writer_data;
	writer_data.data = (unsigned char *) calloc(1, WRITE_BUF_SIZE);
	if (writer_data.data == NULL) {
		CU_FAIL("Could not alloc writebuf.\n");
	}
	writer_data.data_size = WRITE_BUF_SIZE;
	writer_data.pos = 0;
	struct firefly_connection sender_conn;
	sender_conn.transport_conn_platspec = NULL;
	sender_conn.writer_data = &writer_data;
	sender_conn.transport_write = transport_write_udp_posix_mock;

	struct ff_transport_data reader_data;
	reader_data.data = NULL;
	reader_data.data_size = 0;
	reader_data.pos = 0;
	struct firefly_connection receiver_conn;
	receiver_conn.transport_conn_platspec = NULL;
	receiver_conn.reader_data = &reader_data;
	receiver_conn.transport_write = NULL;

	// Construct decoder.
	struct labcomm_decoder *decoder =
		labcomm_decoder_new(ff_transport_reader, &receiver_conn);
	labcomm_register_error_handler_decoder(decoder, handle_labcomm_error);
	labcomm_decoder_register_firefly_protocol_data_sample(decoder,
			handle_firefly_protocol_data_sample, &receiver_conn);
	// Construct encoder.
	struct labcomm_encoder *encoder = labcomm_encoder_new(
			ff_transport_writer, &sender_conn);
	labcomm_register_error_handler_encoder(encoder, handle_labcomm_error);

	// The decoder must have been created before this!
	labcomm_encoder_register_firefly_protocol_data_sample(encoder);
	// Simulate that we're on the other end and wants to decode.
	reader_data.data = malloc(writer_data.pos);
	if (writer_data.data == NULL) {
		CU_FAIL("Could not alloc readbuf.\n");
	}
	memcpy(reader_data.data, writer_data.data, writer_data.pos);
	reader_data.data_size = writer_data.pos;
	labcomm_decoder_decode_one(decoder);

	writer_data.pos = reader_data.pos = 0;

	labcomm_encode_firefly_protocol_data_sample(encoder, &proto);
	free(reader_data.data);
	reader_data.data = malloc(writer_data.pos);
	if (writer_data.data == NULL) {
		CU_FAIL("Could not alloc readbuf.\n");
	}
	memcpy(reader_data.data, writer_data.data, writer_data.pos);
	reader_data.data_size = writer_data.pos;
	labcomm_decoder_decode_one(decoder);

	CU_ASSERT_TRUE(successfully_decoded);
	free(writer_data.data);
	free(reader_data.data);
	labcomm_encoder_free(encoder);
	labcomm_decoder_free(decoder);
	successfully_decoded = false;
}

static size_t nbr_entries = 0;
void transport_write_udp_posix_mock_cmp(unsigned char *data, size_t data_size,
					struct firefly_connection *conn)
{
	size_t file_size;
	unsigned char *file_data;
	if (nbr_entries == 0) {
		file_size = read_file_to_mem(&file_data, SIG_FILE);
	} else {
		file_size = read_file_to_mem(&file_data, DATA_FILE);
	}

	CU_ASSERT_EQUAL(data_size, file_size);
	CU_ASSERT_EQUAL(0, memcmp(data, file_data, data_size));

	free(file_data);
	++nbr_entries;
}

void test_encode_protocol()
{
	firefly_protocol_data_sample proto;
	proto.chan_id = chan_id;
	proto.important = important;
	proto.app_enc_data.n_0 = sizeof(app_data);
	proto.app_enc_data.a = app_data;

	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_data_sample,
		(lc_encode_f) labcomm_encode_firefly_protocol_data_sample, &proto);

	struct ff_transport_data writer_data;
	writer_data.data = (unsigned char *) calloc(1, WRITE_BUF_SIZE);
	if (writer_data.data == NULL) {
		CU_FAIL("Could not alloc writebuf.\n");
	}
	writer_data.data_size = WRITE_BUF_SIZE;
	writer_data.pos = 0;

	struct firefly_connection sender_conn;
	sender_conn.transport_conn_platspec = NULL;
	sender_conn.writer_data = &writer_data;
	sender_conn.transport_write = transport_write_udp_posix_mock_cmp;

	// Construct proto encoder.
	struct labcomm_encoder *proto_encoder = labcomm_encoder_new(
			ff_transport_writer, &sender_conn);
	labcomm_register_error_handler_encoder(proto_encoder,
			handle_labcomm_error);
	// Now mock_cmp will compare signatures.
	labcomm_encoder_register_firefly_protocol_data_sample(proto_encoder);
	memset(writer_data.data, 0, WRITE_BUF_SIZE);
	writer_data.pos = 0;

	// Now mock_cmp will compare enc data.
	labcomm_encode_firefly_protocol_data_sample(proto_encoder, &proto);

	labcomm_encoder_free(proto_encoder);
	free(writer_data.data);
	nbr_entries = 0;
}

void test_decode_protocol()
{
	firefly_protocol_data_sample proto;
	proto.chan_id = chan_id;
	proto.important = important;
	proto.app_enc_data.n_0 = sizeof(app_data);
	proto.app_enc_data.a = app_data;

	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_data_sample,
		(lc_encode_f) labcomm_encode_firefly_protocol_data_sample, &proto);

	struct ff_transport_data reader_data;
	reader_data.pos = 0;
	reader_data.data_size = read_file_to_mem(&reader_data.data, SIG_FILE);

	struct firefly_connection conn;
	conn.transport_conn_platspec = NULL;
	conn.reader_data = &reader_data;
	conn.transport_write = NULL;

	// Construct decoder.
	struct labcomm_decoder *dec =
			labcomm_decoder_new(ff_transport_reader, &conn);
	labcomm_register_error_handler_decoder(dec, handle_labcomm_error);
	labcomm_decoder_register_firefly_protocol_data_sample(dec,
			handle_firefly_protocol_data_sample, &conn);

	// Read sign.
	labcomm_decoder_decode_one(dec);

	free(reader_data.data);
	reader_data.data = NULL;
	reader_data.pos = 0;
	reader_data.data_size = read_file_to_mem(&reader_data.data, DATA_FILE);

	// Read data
	labcomm_decoder_decode_one(dec);

	free(reader_data.data);
	labcomm_decoder_free(dec);
	CU_ASSERT_TRUE(successfully_decoded);
	successfully_decoded = false;

}

static int i = 0;

void handle_firefly_protocol_data_sample_counter(
		firefly_protocol_data_sample *proto, void *context)
{
	CU_ASSERT_EQUAL(proto->chan_id, i);
	CU_ASSERT_EQUAL(proto->important, important);
	CU_ASSERT_EQUAL(proto->app_enc_data.n_0, sizeof(app_data));
	CU_ASSERT_EQUAL(0, memcmp(proto->app_enc_data.a, app_data,
						sizeof(app_data)));
	successfully_decoded = true;
}

void test_decode_protocol_multiple_times()
{
	firefly_protocol_data_sample proto;
	proto.chan_id = i;
	proto.important = important;
	proto.app_enc_data.n_0 = sizeof(app_data);
	proto.app_enc_data.a = app_data;

	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_data_sample,
		(lc_encode_f) labcomm_encode_firefly_protocol_data_sample, &proto);

	struct ff_transport_data reader_data;
	reader_data.pos = 0;
	reader_data.data_size = read_file_to_mem(&reader_data.data, SIG_FILE);

	struct firefly_connection conn;
	conn.transport_conn_platspec = NULL;
	conn.reader_data = &reader_data;
	conn.transport_write = NULL;

	// Construct decoder.
	struct labcomm_decoder *dec = labcomm_decoder_new(ff_transport_reader,
			&conn);
	labcomm_register_error_handler_decoder(dec, handle_labcomm_error);
	labcomm_decoder_register_firefly_protocol_data_sample(dec,
			handle_firefly_protocol_data_sample_counter, &conn);

	// Read sign.
	labcomm_decoder_decode_one(dec);
	free(reader_data.data);
	reader_data.data = NULL;
	reader_data.pos = 0;

	for (i = 0; i < 10; i++) {
		proto.chan_id = i;
		create_labcomm_files_general(
			labcomm_encoder_register_firefly_protocol_data_sample,
			(lc_encode_f)
			labcomm_encode_firefly_protocol_data_sample,
			&proto);
		reader_data.data_size = read_file_to_mem(&reader_data.data,
				DATA_FILE);

		// Read data
		labcomm_decoder_decode_one(dec);

		free(reader_data.data);
		reader_data.data = NULL;
		reader_data.pos = 0;
	}

	labcomm_decoder_free(dec);
	CU_ASSERT_TRUE(successfully_decoded);
	successfully_decoded = false;

}

void test_encode_protocol_multiple_times()
{
	firefly_protocol_data_sample proto;
	proto.chan_id = i;
	proto.important = important;
	proto.app_enc_data.n_0 = sizeof(app_data);
	proto.app_enc_data.a = app_data;

	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_data_sample,
		(lc_encode_f) labcomm_encode_firefly_protocol_data_sample,
		&proto);

	struct ff_transport_data writer_data;
	writer_data.data = (unsigned char *) calloc(1, WRITE_BUF_SIZE);
	if (writer_data.data == NULL) {
		CU_FAIL("Could not alloc writebuf.\n");
	}
	writer_data.data_size = WRITE_BUF_SIZE;
	writer_data.pos = 0;

	struct firefly_connection sender_conn;
	sender_conn.transport_conn_platspec = NULL;
	sender_conn.writer_data = &writer_data;
	sender_conn.transport_write = transport_write_udp_posix_mock_cmp;

	// Construct proto encoder.
	struct labcomm_encoder *proto_encoder = labcomm_encoder_new(
			ff_transport_writer, &sender_conn);
	labcomm_register_error_handler_encoder(proto_encoder,
			handle_labcomm_error);
	// Now mock_cmp will compare signatures.
	labcomm_encoder_register_firefly_protocol_data_sample(proto_encoder);

	for (i = 0; i < 10; i++) {
		proto.chan_id = i;

		create_labcomm_files_general(
			labcomm_encoder_register_firefly_protocol_data_sample,
			(lc_encode_f)
			labcomm_encode_firefly_protocol_data_sample,
			&proto);

		memset(writer_data.data, 0, WRITE_BUF_SIZE);
		writer_data.pos = 0;

		// Now mock_cmp will compare enc data.
		labcomm_encode_firefly_protocol_data_sample(proto_encoder,
								&proto);
	}

	// Clean up
	labcomm_encoder_free(proto_encoder);
	free(writer_data.data);
	nbr_entries = 0;
}

void get_streams()
{
	labcomm_mem_writer_context_t *wmcontext =
			labcomm_mem_writer_context_t_new(0, 0, NULL);
	struct labcomm_encoder *e_encoder = labcomm_encoder_new(
			labcomm_mem_writer, wmcontext);

	struct labcomm_mem_reader_context_t rmcontext;
	struct labcomm_decoder *e_decoder = labcomm_decoder_new(
			labcomm_mem_reader, &rmcontext);

	if (e_encoder == NULL || e_decoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
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

static struct labcomm_decoder *test_dec;
static labcomm_mem_reader_context_t *test_dec_ctx;
static bool sent_chan_req = false;
static bool sent_chan_ack = false;

static void chan_open_handle_ack(firefly_protocol_channel_ack *ack, void *ctx)
{
	struct firefly_channel *chan = ((struct firefly_connection *) ctx)->
		chan_list->chan;
	CU_ASSERT_EQUAL(chan->local_chan_id, ack->source_chan_id);
	CU_ASSERT_EQUAL(chan->remote_chan_id, ack->dest_chan_id);
	CU_ASSERT_EQUAL(chan->remote_chan_id, REMOTE_CHAN_ID);
	CU_ASSERT_TRUE(ack->ack);
	sent_chan_ack = true;
}
static void chan_open_handle_req(firefly_protocol_channel_request *req,
		void *ctx)
{
	struct firefly_channel *chan = ((struct firefly_connection *) ctx)->
		chan_list->chan;
	CU_ASSERT_EQUAL(chan->local_chan_id, req->source_chan_id);
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
	firefly_protocol_channel_request chan_req;
	chan_req.source_chan_id = 1;
	chan_req.dest_chan_id = CHANNEL_ID_NOT_SET;
	firefly_protocol_channel_ack chan_ack;
	chan_ack.ack = true;
	chan_ack.dest_chan_id = REMOTE_CHAN_ID;
	chan_ack.source_chan_id = 1;

	// Init connection and register error handler on encoder and decoder
	struct firefly_connection *conn =
		firefly_connection_new(chan_open_is_open_mock, NULL);
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
	test_dec_ctx->enc_data = NULL;
	test_dec_ctx->size = 0;
	test_dec = labcomm_decoder_new(labcomm_mem_reader, test_dec_ctx);
	if (test_dec == NULL) {
		CU_FAIL("Encoder was null\n");
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

	// Test that a chan open request is sent.
	CU_ASSERT_TRUE(sent_chan_req);
	sent_chan_req = false;

	// Simulate that we received a channel response from the other end
	firefly_protocol_channel_response chan_resp;
	chan_resp.source_chan_id = REMOTE_CHAN_ID;
	chan_resp.dest_chan_id = conn->chan_list->chan->local_chan_id;
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
	CU_ASSERT_EQUAL(chan->remote_chan_id, REMOTE_CHAN_ID);
	chan_accept_called = true;
	return true;
}

void chan_recv_handle_res(firefly_protocol_channel_response *res, void *ctx)
{
	struct firefly_connection *conn = (struct firefly_connection *) ctx;
	struct firefly_channel *chan = conn->chan_list->chan;
	CU_ASSERT_EQUAL(res->dest_chan_id, REMOTE_CHAN_ID);
	CU_ASSERT_EQUAL(chan->remote_chan_id, REMOTE_CHAN_ID);
	CU_ASSERT_EQUAL(chan->local_chan_id, res->source_chan_id);
	sent_response = true;
}

void chan_recv_transport_write_mock(unsigned char *data, size_t size,
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
	CU_ASSERT_EQUAL(res->source_chan_id, accepted_chan->local_chan_id);
	CU_ASSERT_TRUE(res->ack);
}

void test_chan_recv_accept()
{
	// Init connection and register error handler on encoder and decoder
	struct firefly_connection *conn =
		firefly_connection_new(chan_recv_chan_opened_mock,
				chan_recv_accept_chan);
	if (conn == NULL) {
		CU_FAIL("Could not create connection.\n");
	}
	// Set some stuff that firefly_init_conn doesn't do
	conn->transport_conn_platspec = NULL;
	conn->transport_write = chan_recv_transport_write_mock;
	labcomm_register_error_handler_encoder(conn->transport_encoder,
			handle_labcomm_error);
	labcomm_register_error_handler_decoder(conn->transport_decoder,
			handle_labcomm_error);
	//
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

	// Test accept called.
	CU_ASSERT_TRUE(chan_accept_called);
	chan_accept_called = false; // Reset value.

	CU_ASSERT_TRUE(sent_response);
	sent_response = false;

	// simulate an ACK is sent from remote
	firefly_protocol_channel_ack ack;
	ack.dest_chan_id = conn->chan_list->chan->local_chan_id;
	ack.source_chan_id = conn->chan_list->chan->remote_chan_id;
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

	CU_ASSERT_TRUE(chan_opened_called);
	free(sign);
	free(data);
	free(ack_sign);
	free(ack_data);
	labcomm_decoder_free(test_dec);
	free(test_dec_ctx);
	firefly_connection_free(&conn);
}

int main()
{
	CU_pSuite tran_enc_dec_suite = NULL;
	CU_pSuite chan_suite = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	// Add our test suites.
	tran_enc_dec_suite = CU_add_suite("transport_enc_dec",
					init_suit, clean_suit);
	if (tran_enc_dec_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	chan_suite = CU_add_suite("chan_suite", init_suit, clean_suit);
	if (chan_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Transport encoding and decoding tests.
	if (
		(CU_add_test(tran_enc_dec_suite, "test_encode_protocol",
			     test_encode_protocol) == NULL)
		||
		(CU_add_test(tran_enc_dec_suite, "test_decode_protocol",
			     test_decode_protocol) == NULL)
		||
		(CU_add_test(tran_enc_dec_suite, "test_encode_decode_protocol",
			     test_encode_decode_protocol) == NULL)
		||
		(CU_add_test(tran_enc_dec_suite,
			     "test_decode_protocol_multiple_times",
			     test_decode_protocol_multiple_times) == NULL)
		||
		(CU_add_test(tran_enc_dec_suite,
			     "test_encode_protocol_multiple_times",
			     test_encode_protocol_multiple_times) == NULL)
	   ) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Channel tests.
	if (
		(CU_add_test(chan_suite, "get_streams",
			     get_streams) == NULL)
		||
		(CU_add_test(chan_suite, "test_chan_open",
			     test_chan_open) == NULL)
		||
		(CU_add_test(chan_suite, "test_chan_recv_accept",
			     test_chan_recv_accept) == NULL)
	   ) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Set verbosity.
	CU_basic_set_mode(CU_BRM_VERBOSE);
	/*CU_console_run_tests();*/

	// Run all test suites.
	CU_basic_run_tests();


	// Clean up.
	CU_cleanup_registry();

	return CU_get_error();
}
