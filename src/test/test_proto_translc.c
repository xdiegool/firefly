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

#include "test/test_labcomm_utils.h"

#include "test/test_proto_chan.h"

#define DATA_FILE	("testfiles/data.enc")	/* Encoded test data. */
#define SIG_FILE	("testfiles/sig.enc")	/* uncoded test signature. */
#define SRC_CHAN_ID	(3)			/* Source channe ID. */
#define DEST_CHAN_ID	(1)			/* Desination channel ID. */
#define WRITE_BUF_SIZE (128)	/* Size of the LabComm buffer to write to. */

int init_suit_translc()
{
	return 0; // Success.
}

int clean_suit_translc()
{
	return 0; // Success.
}

static bool important = true;
static unsigned char app_data[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
static bool successfully_decoded = false;

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
static size_t last_written_size = 0;
void transport_write_udp_posix_mock(unsigned char *data, size_t data_size,
		struct firefly_connection *conn)
{
	unsigned char *zero_buf = calloc(1, WRITE_BUF_SIZE);
	// The buffer shoule not be empty anymore.
	CU_ASSERT_NOT_EQUAL(0, memcmp(data, zero_buf, WRITE_BUF_SIZE));
	free(zero_buf);
	last_written_size = data_size;
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
	reader_data.data = malloc(last_written_size);
	if (writer_data.data == NULL) {
		CU_FAIL("Could not alloc readbuf.\n");
	}
	memcpy(reader_data.data, writer_data.data, last_written_size);
	reader_data.data_size = last_written_size;
	labcomm_decoder_decode_one(decoder);

	reader_data.pos = 0;

	labcomm_encode_firefly_protocol_data_sample(encoder, &data_sample);
	free(reader_data.data);
	reader_data.data = malloc(last_written_size);
	if (writer_data.data == NULL) {
		CU_FAIL("Could not alloc readbuf.\n");
	}
	memcpy(reader_data.data, writer_data.data, last_written_size);
	reader_data.data_size = last_written_size;
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
	firefly_protocol_data_sample data_sample;
	data_sample.src_chan_id = SRC_CHAN_ID;
	data_sample.dest_chan_id = DEST_CHAN_ID;
	data_sample.important = important;
	data_sample.app_enc_data.n_0 = sizeof(app_data);
	data_sample.app_enc_data.a = app_data;

	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_data_sample,
		(lc_encode_f) labcomm_encode_firefly_protocol_data_sample,
		&data_sample);

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

	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_data_sample,
		(lc_encode_f) labcomm_encode_firefly_protocol_data_sample,
		&data_sample);

	struct ff_transport_data reader_data;
	reader_data.pos = 0;
	reader_data.data_size = read_file_to_mem(&reader_data.data,
						SIG_FILE);

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
							DATA_FILE);

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

	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_data_sample,
		(lc_encode_f) labcomm_encode_firefly_protocol_data_sample,
		&data_sample);

	struct ff_transport_data reader_data;
	reader_data.pos = 0;
	reader_data.data_size = read_file_to_mem(&reader_data.data,
							SIG_FILE);

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
		create_labcomm_files_general(
			labcomm_encoder_register_firefly_protocol_data_sample,
			(lc_encode_f)
			labcomm_encode_firefly_protocol_data_sample,
			&data_sample);
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
	firefly_protocol_data_sample data_sample;
	data_sample.src_chan_id = i;
	data_sample.dest_chan_id = i;
	data_sample.important = important;
	data_sample.app_enc_data.n_0 = sizeof(app_data);
	data_sample.app_enc_data.a = app_data;

	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_data_sample,
		(lc_encode_f) labcomm_encode_firefly_protocol_data_sample,
		&data_sample);

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

		create_labcomm_files_general(
			labcomm_encoder_register_firefly_protocol_data_sample,
			(lc_encode_f)
			labcomm_encode_firefly_protocol_data_sample,
			&data_sample);

		memset(writer_data.data, 0, WRITE_BUF_SIZE);

		// Now mock_cmp will compare enc data.
		labcomm_encode_firefly_protocol_data_sample(proto_encoder,
								&data_sample);
	}

	// Clean up
	labcomm_encoder_free(proto_encoder);
	free(writer_data.data);
	nbr_entries = 0;
}
