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
#include <gen/test.h>
#include <firefly_errors.h>
#include "protocol/firefly_protocol_private.h"
#include <protocol/firefly_protocol.h>

#define SRC_CHAN_ID	(3)	/* Source channe ID. */
#define DEST_CHAN_ID	(1)	/* Desination channel ID. */
#define WRITE_BUF_SIZE (256)	// Size of the LabComm buffer to write to.
#define DATA_FILE_FIREFLY ("data_firefly.enc")	// Encoded firefly data.
#define SIG_FILE_FIREFLY ("sig_firefly.enc")	// Encoded firefly signature.
#define DATA_FILE_TEST ("data_test.enc")	// Encoded test data.
#define SIG_FILE_TEST ("sig_test.enc")		// Encoded test signature.

int init_suit()
{
	return 0; // Success.
}

int clean_suit()
{
	return 0; // Success.
}

static bool important = true;
static unsigned char app_data[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
static bool successfully_decoded = false;

void handle_labcomm_error(enum labcomm_error error_id, size_t nbr_va_args, ...)
{
	const char *err_msg = labcomm_error_get_str(error_id);
	if (err_msg == NULL) {
		err_msg = "Error with an unknown error ID occured.";
	}
	firefly_error(FIREFLY_ERROR_LABCOMM, 1, err_msg);

	CU_FAIL("Labcomm error occured!\n");
}

void create_labcomm_files_chan_req(firefly_protocol_channel_request *cr)
{
	int tmpfd = open(SIG_FILE_FIREFLY, O_RDWR|O_CREAT|O_TRUNC, 0644);
	struct labcomm_encoder *fd_encoder =
			labcomm_encoder_new(labcomm_fd_writer, &tmpfd);
	if (fd_encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_encoder_register_firefly_protocol_channel_request(fd_encoder);
	close(tmpfd);
	tmpfd = open(DATA_FILE_FIREFLY, O_RDWR|O_CREAT|O_TRUNC, 0644);
	labcomm_encode_firefly_protocol_channel_request(fd_encoder, cr);
	close(tmpfd);
	labcomm_encoder_free(fd_encoder);
}

void create_labcomm_files_proto(firefly_protocol_data_sample *data_sample)
{
	int tmpfd = open(SIG_FILE_FIREFLY, O_RDWR|O_CREAT|O_TRUNC, 0644);
	struct labcomm_encoder *fd_encoder = labcomm_encoder_new(
					labcomm_fd_writer, &tmpfd);
	if (fd_encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_encoder(fd_encoder, handle_labcomm_error);
	labcomm_encoder_register_firefly_protocol_data_sample(fd_encoder);
	close(tmpfd);
	tmpfd = open(DATA_FILE_FIREFLY, O_RDWR|O_CREAT|O_TRUNC, 0644);
	labcomm_encode_firefly_protocol_data_sample(fd_encoder, data_sample);
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
		perror("ftell fcked.\n");
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

void handle_firefly_protocol_data_sample(
		firefly_protocol_data_sample *data_sample, void *context)
{
	CU_ASSERT_EQUAL(data_sample->src_chan_id, SRC_CHAN_ID);
	CU_ASSERT_EQUAL(data_sample->dest_chan_id, DEST_CHAN_ID);
	CU_ASSERT_EQUAL(data_sample->important, important);
	CU_ASSERT_EQUAL(data_sample->app_enc_data.n_0, sizeof(app_data));
	CU_ASSERT_EQUAL(0, memcmp(data_sample->app_enc_data.a, app_data,
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
	firefly_protocol_data_sample data_sample;
	data_sample.src_chan_id = SRC_CHAN_ID;
	data_sample.dest_chan_id = DEST_CHAN_ID;
	data_sample.important = important;
	data_sample.app_enc_data.n_0 = sizeof(app_data);
	data_sample.app_enc_data.a = app_data;

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
	if (decoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_decoder(decoder, handle_labcomm_error);
	labcomm_decoder_register_firefly_protocol_data_sample(decoder,
			handle_firefly_protocol_data_sample, &receiver_conn);
	// Construct encoder.
	struct labcomm_encoder *encoder = labcomm_encoder_new(
			ff_transport_writer, &sender_conn);
	if (encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
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

	labcomm_encode_firefly_protocol_data_sample(encoder, &data_sample);
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
		file_size = read_file_to_mem(&file_data, SIG_FILE_FIREFLY);
	} else {
		file_size = read_file_to_mem(&file_data, DATA_FILE_FIREFLY);
	}

	CU_ASSERT_EQUAL(data_size, file_size);
	CU_ASSERT_EQUAL(0, memcmp(data, file_data, data_size));

	free(file_data);
	++nbr_entries;
}

void test_encode_protocol()
{
	firefly_protocol_data_sample data_sample;
	data_sample.src_chan_id = SRC_CHAN_ID;
	data_sample.dest_chan_id = DEST_CHAN_ID;
	data_sample.important = important;
	data_sample.app_enc_data.n_0 = sizeof(app_data);
	data_sample.app_enc_data.a = app_data;

	create_labcomm_files_proto(&data_sample);

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

	// Construct data_sample encoder.
	struct labcomm_encoder *proto_encoder = labcomm_encoder_new(
			ff_transport_writer, &sender_conn);
	if (proto_encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_encoder(proto_encoder,
			handle_labcomm_error);
	// Now mock_cmp will compare signatures.
	labcomm_encoder_register_firefly_protocol_data_sample(proto_encoder);
	memset(writer_data.data, 0, WRITE_BUF_SIZE);
	writer_data.pos = 0;

	// Now mock_cmp will compare enc data.
	labcomm_encode_firefly_protocol_data_sample(proto_encoder, &data_sample);

	labcomm_encoder_free(proto_encoder);
	free(writer_data.data);
	nbr_entries = 0;
}

void test_decode_protocol()
{
	firefly_protocol_data_sample data_sample;
	data_sample.src_chan_id = SRC_CHAN_ID;
	data_sample.dest_chan_id = DEST_CHAN_ID;
	data_sample.important = important;
	data_sample.app_enc_data.n_0 = sizeof(app_data);
	data_sample.app_enc_data.a = app_data;

	create_labcomm_files_proto(&data_sample);

	struct ff_transport_data reader_data;
	reader_data.pos = 0;
	reader_data.data_size = read_file_to_mem(&reader_data.data,
						SIG_FILE_FIREFLY);

	struct firefly_connection conn;
	conn.transport_conn_platspec = NULL;
	conn.reader_data = &reader_data;
	conn.transport_write = NULL;

	// Construct decoder.
	struct labcomm_decoder *dec =
			labcomm_decoder_new(ff_transport_reader, &conn);
	if (dec == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_decoder(dec, handle_labcomm_error);
	labcomm_decoder_register_firefly_protocol_data_sample(dec,
			handle_firefly_protocol_data_sample, &conn);

	// Read sign.
	labcomm_decoder_decode_one(dec);

	free(reader_data.data);
	reader_data.data = NULL;
	reader_data.pos = 0;
	reader_data.data_size = read_file_to_mem(&reader_data.data,
							DATA_FILE_FIREFLY);

	// Read data
	labcomm_decoder_decode_one(dec);

	free(reader_data.data);
	labcomm_decoder_free(dec);
	CU_ASSERT_TRUE(successfully_decoded);
	successfully_decoded = false;

}

static int i = 0;

void handle_firefly_protocol_data_sample_counter(
		firefly_protocol_data_sample *data_sample, void *context)
{
	CU_ASSERT_EQUAL(data_sample->src_chan_id, i);
	CU_ASSERT_EQUAL(data_sample->dest_chan_id, i);
	CU_ASSERT_EQUAL(data_sample->important, important);
	CU_ASSERT_EQUAL(data_sample->app_enc_data.n_0, sizeof(app_data));
	CU_ASSERT_EQUAL(0, memcmp(data_sample->app_enc_data.a, app_data,
						sizeof(app_data)));
	successfully_decoded = true;
}

void test_decode_protocol_multiple_times()
{
	firefly_protocol_data_sample data_sample;
	data_sample.src_chan_id = i;
	data_sample.dest_chan_id = i;
	data_sample.important = important;
	data_sample.app_enc_data.n_0 = sizeof(app_data);
	data_sample.app_enc_data.a = app_data;

	create_labcomm_files_proto(&data_sample);

	struct ff_transport_data reader_data;
	reader_data.pos = 0;
	reader_data.data_size = read_file_to_mem(&reader_data.data,
							SIG_FILE_FIREFLY);

	struct firefly_connection conn;
	conn.transport_conn_platspec = NULL;
	conn.reader_data = &reader_data;
	conn.transport_write = NULL;

	// Construct decoder.
	struct labcomm_decoder *dec = labcomm_decoder_new(ff_transport_reader,
			&conn);
	if (dec == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_decoder(dec, handle_labcomm_error);
	labcomm_decoder_register_firefly_protocol_data_sample(dec,
			handle_firefly_protocol_data_sample_counter, &conn);

	// Read sign.
	labcomm_decoder_decode_one(dec);
	free(reader_data.data);
	reader_data.data = NULL;
	reader_data.pos = 0;

	for (i = 0; i < 10; i++) {
		data_sample.src_chan_id = i;
		data_sample.dest_chan_id = i;
		create_labcomm_files_proto(&data_sample);
		reader_data.data_size = read_file_to_mem(&reader_data.data,
				DATA_FILE_FIREFLY);

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
	firefly_protocol_data_sample data_sample;
	data_sample.src_chan_id = i;
	data_sample.dest_chan_id = i;
	data_sample.important = important;
	data_sample.app_enc_data.n_0 = sizeof(app_data);
	data_sample.app_enc_data.a = app_data;

	create_labcomm_files_proto(&data_sample);

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

	// Construct data_sample encoder.
	struct labcomm_encoder *proto_encoder = labcomm_encoder_new(
			ff_transport_writer, &sender_conn);
	if (proto_encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_encoder(proto_encoder,
			handle_labcomm_error);
	// Now mock_cmp will compare signatures.
	labcomm_encoder_register_firefly_protocol_data_sample(proto_encoder);

	for (i = 0; i < 10; i++) {
		data_sample.src_chan_id = i;

		create_labcomm_files_proto(&data_sample);

		memset(writer_data.data, 0, WRITE_BUF_SIZE);
		writer_data.pos = 0;

		// Now mock_cmp will compare enc data.
		labcomm_encode_firefly_protocol_data_sample(proto_encoder,
								&data_sample);
	}

	// Clean up
	labcomm_encoder_free(proto_encoder);
	free(writer_data.data);
	nbr_entries = 0;
}

void test_get_streams()
{
	// Construct encoder.
	labcomm_mem_writer_context_t *wmcontext =
		labcomm_mem_writer_context_t_new(0, 0, NULL);
	struct labcomm_encoder *e_encoder = labcomm_encoder_new(
			labcomm_mem_writer, wmcontext);
	if (e_encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_encoder(e_encoder, handle_labcomm_error);

	// Construct decoder.
	struct labcomm_mem_reader_context_t rmcontext;
	struct labcomm_decoder *e_decoder = labcomm_decoder_new(
			labcomm_mem_reader, &rmcontext);
	if (e_decoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
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

static struct firefly_channel *test_chan_open_chan;
static bool chan_opened_called = false;
void chan_open_is_open_mock(struct firefly_channel *chan)
{
	// test
	test_chan_open_chan = chan;
	test_chan_open_chan->proto_encoder = NULL;
	test_chan_open_chan->proto_decoder = NULL;
	chan_opened_called = true;
}

static bool chan_open_req_sent = false;
void chan_open_transport_write_mock(unsigned char *data, size_t size,
		struct firefly_connection *conn)
{
	transport_write_udp_posix_mock_cmp(data, size, conn);
	if(nbr_entries != 0) {
		chan_open_req_sent = true;
	}
}

void test_chan_open()
{
	firefly_protocol_channel_request chan_req;
	chan_req.source_chan_id = 1;
	chan_req.dest_chan_id = 0;
	chan_req.ack = true;

	create_labcomm_files_chan_req(&chan_req);

	struct ff_transport_data writer_data;
	writer_data.data = malloc(128);
	writer_data.data_size = 128;
	writer_data.pos = 0;

	struct firefly_connection conn;
	conn.transport_conn_platspec = NULL;
	conn.writer_data = &writer_data;
	conn.transport_write = chan_open_transport_write_mock;
	conn.on_channel_opened = chan_open_is_open_mock;
	conn.chan_list = NULL;
	conn.transport_encoder =
		labcomm_encoder_new(ff_transport_writer, &conn);
	if (conn.transport_encoder == NULL) {
		CU_FAIL("Encoder was null\n");
	}
	labcomm_encoder_register_firefly_protocol_channel_request(
			conn.transport_encoder);
	free(writer_data.data);
	writer_data.data = NULL;
	writer_data.data_size = 0;
	writer_data.pos = 0;

	struct ff_transport_data reader_data;
	reader_data.data = NULL;
	reader_data.data_size = 0;
	reader_data.pos = 0;
	conn.reader_data = &reader_data;
	conn.transport_decoder = labcomm_decoder_new(ff_transport_reader, &conn);
	labcomm_decoder_register_firefly_protocol_channel_request(
			conn.transport_decoder, handle_channel_request, &conn);

	// Test that a chan open request is sent.
	firefly_channel_open(&conn);
	CU_ASSERT_TRUE(chan_open_req_sent);
	nbr_entries = 1;
	chan_open_req_sent = false;

	// Simulate receive ack
	chan_req.source_chan_id = 2;
	chan_req.dest_chan_id = 1;
	chan_req.ack = true;

	create_labcomm_files_chan_req(&chan_req);

	unsigned char *sign;
	unsigned char *data;
	size_t sign_size = read_file_to_mem(&sign, SIG_FILE_FIREFLY);
	size_t data_size = read_file_to_mem(&data, DATA_FILE_FIREFLY);

	chan_req.source_chan_id = 1;
	chan_req.dest_chan_id = 2;
	chan_req.ack = true;

	create_labcomm_files_chan_req(&chan_req);

	protocol_data_received(&conn, sign, sign_size);
	protocol_data_received(&conn, data, data_size);
	// check if last ACK is sent
	CU_ASSERT_TRUE(chan_open_req_sent);

	// Check ACK is sent

	// test chan_is_open is called
	CU_ASSERT_TRUE(chan_opened_called);

	// Clean up
	chan_open_req_sent = false;
	chan_opened_called = false;
	free(sign);
	free(data);
	nbr_entries = 0;
	labcomm_decoder_free(conn.transport_decoder);
	labcomm_encoder_free(conn.transport_encoder);
	free(conn.writer_data->data);
	// TODO must clean up chan better here, it's a quick fix to make this
	// test case leak free.
	firefly_channel_close(&test_chan_open_chan, &conn);
}

void create_labcomm_files_test(test_test_var *test)
{
	int tmpfd = open(SIG_FILE_TEST, O_RDWR|O_CREAT|O_TRUNC, 0644);
	struct labcomm_encoder *fd_encoder = labcomm_encoder_new(
						labcomm_fd_writer, &tmpfd);
	if (fd_encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_encoder(fd_encoder, handle_labcomm_error);
	labcomm_encoder_register_test_test_var(fd_encoder);
	close(tmpfd);
	tmpfd = open(DATA_FILE_TEST, O_RDWR|O_CREAT|O_TRUNC, 0644);
	labcomm_encode_test_test_var(fd_encoder, test);
	close(tmpfd);
	labcomm_encoder_free(fd_encoder);
}

static size_t proto_check_cnt = 0;

void proto_check_writer(unsigned char *data, size_t data_size,
			struct firefly_connection *conn)
{
	if (proto_check_cnt == 0) {
		// DO nothing
	} else if (proto_check_cnt == 1 || proto_check_cnt == 2) {
		// Test sig.
		size_t file_size;
		unsigned char *file_data;
		file_size = read_file_to_mem(&file_data, DATA_FILE_FIREFLY);

		CU_ASSERT_EQUAL(data_size, file_size);
		CU_ASSERT_EQUAL(0, memcmp(data, file_data, data_size));

		free(file_data);
	} else {
		CU_FAIL("You should not be here...\n");
	}
	++proto_check_cnt;
}


void test_proto_writer()
{
	// Prepare the expected data.
	test_test_var var_test_test = 42;
	create_labcomm_files_test(&var_test_test);
	unsigned char *test_sig_buf;
	unsigned char *test_data_buf;
	size_t proto_sig_size = read_file_to_mem(&test_sig_buf, SIG_FILE_TEST);
	size_t proto_data_size = read_file_to_mem(&test_data_buf, DATA_FILE_TEST);

	// Protocol packet for the sig.
	firefly_protocol_data_sample data_sample_sig;
	data_sample_sig.dest_chan_id = DEST_CHAN_ID;
	data_sample_sig.src_chan_id = SRC_CHAN_ID;
	data_sample_sig.important = important;
	data_sample_sig.app_enc_data.a = test_sig_buf;
	data_sample_sig.app_enc_data.n_0 = proto_sig_size;

	// Protocol packet for the data.
	firefly_protocol_data_sample data_sample_data;
	data_sample_data.dest_chan_id = DEST_CHAN_ID;
	data_sample_data.src_chan_id = SRC_CHAN_ID;
	data_sample_data.important = important;
	data_sample_data.app_enc_data.a = test_data_buf;
	data_sample_data.app_enc_data.n_0 = proto_data_size;

	// Prepare for the test.
	struct firefly_connection conn;
	conn.transport_conn_platspec = NULL;
	conn.writer_data = malloc(sizeof(struct ff_transport_data));
	if (conn.writer_data == NULL) {
		CU_FAIL("Could not allocate conn.writer_data.");
	}
	conn.writer_data->data = malloc(WRITE_BUF_SIZE);
	if (conn.writer_data->data == NULL) {
		CU_FAIL("Could not allocate conn.writer_data->data.");
	}
	conn.writer_data->data_size = WRITE_BUF_SIZE;
	conn.writer_data->pos = 0;
	conn.reader_data = NULL;
	conn.transport_write = proto_check_writer;

	struct labcomm_encoder *transport_encoder = labcomm_encoder_new(
				ff_transport_writer, &conn);
	if (transport_encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_encoder(transport_encoder,
			handle_labcomm_error);
	labcomm_encoder_register_firefly_protocol_data_sample(transport_encoder);
	conn.transport_encoder = transport_encoder;
	conn.writer_data->pos = 0;

	struct firefly_channel chan;
	chan.conn = &conn;
	chan.local_id = SRC_CHAN_ID;
	chan.remote_id = DEST_CHAN_ID;
	struct ff_transport_data writer_data;
	writer_data.data = (unsigned char *) calloc(1, WRITE_BUF_SIZE);
	if (writer_data.data == NULL) {
		CU_FAIL("Could not alloc writebuf.\n");
	}
	writer_data.data_size = WRITE_BUF_SIZE;
	writer_data.pos = 0;
	chan.writer_data = &writer_data;
	struct labcomm_encoder *encoder_proto = labcomm_encoder_new(
			protocol_writer, &chan);
	if (encoder_proto == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_encoder(encoder_proto, handle_labcomm_error);
	chan.proto_encoder = encoder_proto;
	chan.proto_decoder = NULL;

	// Test signature packet.
	create_labcomm_files_proto(&data_sample_sig);
	labcomm_encoder_register_test_test_var(encoder_proto);
	conn.writer_data->pos = 0;

	// Test sample packet.
	create_labcomm_files_proto(&data_sample_data);
	labcomm_encode_test_test_var(encoder_proto, &var_test_test);
	conn.writer_data->pos = 0;

	CU_ASSERT_EQUAL(proto_check_cnt, 3);

	// Free all resources.
	labcomm_encoder_free(transport_encoder);
	free(writer_data.data);
	labcomm_encoder_free(encoder_proto);
	free(conn.writer_data->data);
	free(conn.writer_data);
	free(test_sig_buf);
	free(test_data_buf);
}

static bool handled_test_var = false;
void handle_test_var(test_test_var *a_var, void *context)
{
	test_test_var *e_var = (test_test_var *) context;
	CU_ASSERT_EQUAL(*a_var, *e_var);
	handled_test_var = true;
}

void test_proto_reader()
{
	// Prepare the expected data.
	test_test_var var_test_test = 42;
	create_labcomm_files_test(&var_test_test);
	unsigned char *test_sig_buf;
	unsigned char *test_data_buf;
	size_t proto_sig_size = read_file_to_mem(&test_sig_buf, SIG_FILE_TEST);
	size_t proto_data_size = read_file_to_mem(&test_data_buf,
					DATA_FILE_TEST);

	struct ff_transport_data reader_data;
	struct firefly_channel chan;
	chan.reader_data = &reader_data;
	chan.reader_data->data = test_sig_buf;
	chan.reader_data->data_size = proto_sig_size;
	chan.reader_data->pos = 0;

	struct labcomm_decoder *proto_decoder = labcomm_decoder_new(
						protocol_reader, &chan);
	if (proto_decoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_decoder(proto_decoder,
						handle_labcomm_error);
	labcomm_decoder_register_test_test_var(proto_decoder, handle_test_var,
							&var_test_test);

	labcomm_decoder_decode_one(proto_decoder);

	chan.reader_data->data = test_data_buf;
	chan.reader_data->data_size = proto_data_size;
	chan.reader_data->pos = 0;

	labcomm_decoder_decode_one(proto_decoder);

	CU_ASSERT_TRUE(handled_test_var);

	labcomm_decoder_free(proto_decoder);
	free(test_sig_buf);
	free(test_data_buf);
}

int main()
{
	CU_pSuite tran_enc_dec_suite = NULL;
	CU_pSuite proto_enc_dec_suite = NULL;
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
	proto_enc_dec_suite = CU_add_suite("protocol_enc_dec", init_suit,
							clean_suit);
	if (proto_enc_dec_suite == NULL) {
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

	// Protocol writer/reader tests.
	if (
		(CU_add_test(proto_enc_dec_suite, "test_proto_writer",
			     test_proto_writer) == NULL)
		||
		(CU_add_test(proto_enc_dec_suite, "test_proto_reader",
			     test_proto_reader) == NULL)
	   ) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Channel tests.
	if (
		(CU_add_test(chan_suite, "test_get_streams",
			     test_get_streams) == NULL)
		||
		(CU_add_test(chan_suite, "test_chan_open",
			     test_chan_open) == NULL)
		/*||*/
		/*(CU_add_test(chan_suite, "name",*/
			     /*name) == NULL)*/
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
