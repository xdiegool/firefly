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
#include <labcomm_default_memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <gen/firefly_protocol.h>
#include <gen/test.h>
#include <utils/firefly_errors.h>
#include <protocol/firefly_protocol.h>

#include "protocol/firefly_protocol_private.h"
#include "test/test_labcomm_utils.h"
#include "test/labcomm_static_buffer_writer.h"
#include "test/event_helper.h"
#include "utils/cppmacros.h"

#define NBR_ENCODE_DECODE (100)

int init_suit_labcomm()
{
	return 0; // Success.
}

int clean_suit_labcomm()
{
	return 0; // Success.
}

static bool successfully_decoded = false;
void labcomm_handle_test_test_var(test_test_var *v, void *context)
{
	CU_ASSERT_EQUAL(*v, *((int *) context));
	successfully_decoded = true;
}

void labcomm_handle_data_sample(firefly_protocol_data_sample *v, void *context)
{
	UNUSED_VAR(context);
	UNUSED_VAR(v);
	struct firefly_channel *chan = context;
	labcomm_decoder_ioctl(chan->proto_decoder,
			FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER,
			v->app_enc_data.a, v->app_enc_data.n_0);
	labcomm_decoder_decode_one(chan->proto_decoder);
}

// Simulate that the data is sent over the net an arrives the same way
// on the other end. Now we decoce it and hope it's the same stuff we
// previously encoded.
static size_t last_written_size = 0;
void transport_write_udp_posix_mock(unsigned char *data, size_t data_size,
		struct firefly_connection *conn, bool important, unsigned char *id)
{
	UNUSED_VAR(important);
	UNUSED_VAR(id);
	unsigned char *cpy_data = malloc(data_size);
	memcpy(cpy_data, data, data_size);
	labcomm_decoder_ioctl(conn->transport_decoder,
			FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER, cpy_data, data_size);
	labcomm_decoder_decode_one(conn->transport_decoder);
	last_written_size = data_size;
}

static struct firefly_transport_connection test_trsp_conn = {
	.write = transport_write_udp_posix_mock,
	.ack = NULL,
	.open = NULL,
	.close = NULL,
	.context = NULL
};

void test_encode_decode_protocol()
{
	struct firefly_connection conn;
	conn.transport = &test_trsp_conn;
	conn.memory_replacements.alloc_replacement = NULL;
	conn.memory_replacements.free_replacement = NULL;

	// Construct decoder.
	struct labcomm_reader *r;
	struct labcomm_decoder *decoder;
	r = transport_labcomm_reader_new(&conn, labcomm_default_memory);
	decoder = labcomm_decoder_new(r, NULL, labcomm_default_memory, NULL);
	if (decoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	// TODO: Fix when labcomm gets error handling back
	/* labcomm_register_error_handler_decoder(decoder, handle_labcomm_error);*/

	conn.transport_decoder = decoder;
	// Construct encoder.
	struct labcomm_writer *w;
	struct labcomm_encoder *encoder;
	w = transport_labcomm_writer_new(&conn, labcomm_default_memory);
	encoder = labcomm_encoder_new(w, NULL, labcomm_default_memory, NULL);
	if (encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	// TODO: Fix when labcomm gets error handling back
	/* labcomm_register_error_handler_encoder(encoder, handle_labcomm_error);*/
	conn.transport_encoder = encoder;

	// The decoder must have been created before this!
	int i;
	labcomm_decoder_register_test_test_var(decoder,
			labcomm_handle_test_test_var, &i);
	labcomm_encoder_register_test_test_var(encoder);

	test_test_var v;
	for (i = 0; i < NBR_ENCODE_DECODE; i++) {
		v = i;
		successfully_decoded = false;
		labcomm_encode_test_test_var(encoder, &v);
		CU_ASSERT_TRUE(successfully_decoded);
	}

	CU_ASSERT_TRUE(successfully_decoded);
	labcomm_encoder_free(encoder);
	labcomm_decoder_free(decoder);
	successfully_decoded = false;
}

void test_encode_decode_app()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			10, NULL);
	struct firefly_connection conn;
	conn.transport = &test_trsp_conn;
	conn.open = FIREFLY_CONNECTION_OPEN;
	conn.memory_replacements.alloc_replacement = NULL;
	conn.memory_replacements.free_replacement = NULL;

	// Construct decoder.
	struct labcomm_reader *r;
	r = transport_labcomm_reader_new(&conn, labcomm_default_memory);
	conn.transport_decoder =
			labcomm_decoder_new(r, NULL, labcomm_default_memory, NULL);
	if (conn.transport_decoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	// TODO: Fix when labcomm gets error handling back
	/* labcomm_register_error_handler_decoder(conn.transport_decoder, handle_labcomm_error);*/

	// Construct encoder.
	struct labcomm_writer *w;
	w = transport_labcomm_writer_new(&conn, labcomm_default_memory);
	conn.transport_encoder =
		labcomm_encoder_new(w, NULL, labcomm_default_memory, NULL);
	if (conn.transport_encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	// TODO: Fix when labcomm gets error handling back
	/* labcomm_register_error_handler_encoder(conn.transport_encoder, handle_labcomm_error);*/

	conn.event_queue = eq;

	struct firefly_channel chan = {0};

	r = protocol_labcomm_reader_new(&conn, labcomm_default_memory);
	chan.proto_decoder =
		labcomm_decoder_new(r, NULL, labcomm_default_memory, NULL);
	w = protocol_labcomm_writer_new(&chan, labcomm_default_memory);
	chan.proto_encoder =
		labcomm_encoder_new(w, NULL, labcomm_default_memory, NULL);
	chan.local_id			= 1;
	chan.remote_id			= chan.local_id;
	chan.important_queue	= NULL;
	chan.important_id		= 0;
	chan.current_seqno		= 0;
	chan.remote_seqno		= 0;
	chan.conn               = &conn;

	labcomm_decoder_register_firefly_protocol_data_sample(conn.transport_decoder,
			labcomm_handle_data_sample, &chan);
	labcomm_encoder_register_firefly_protocol_data_sample(conn.transport_encoder);

	int i;
	labcomm_decoder_register_test_test_var(chan.proto_decoder,
			labcomm_handle_test_test_var, &i);
	labcomm_encoder_register_test_test_var(chan.proto_encoder);

	test_test_var v;
	for (i = 0; i < NBR_ENCODE_DECODE; i++) {
		v = i;
		successfully_decoded = false;
		labcomm_encode_test_test_var(chan.proto_encoder, &v);
		event_execute_all_test(eq);
		CU_ASSERT_TRUE(successfully_decoded);
	}

	CU_ASSERT_TRUE(successfully_decoded);
	labcomm_encoder_free(chan.proto_encoder);
	labcomm_decoder_free(chan.proto_decoder);
	labcomm_encoder_free(conn.transport_encoder);
	labcomm_decoder_free(conn.transport_decoder);
	successfully_decoded = false;
	firefly_event_queue_free(&eq);
}

static int nbr_test_vars = 0;
void test_fragments_handle_test_var(test_test_var *v, void *c)
{
	/*printf(" test_test_var: %d ", *v);*/
	CU_ASSERT_EQUAL(*v, nbr_test_vars++);
}

void test_decode_large_protocol_fragments()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			10, NULL);
	struct firefly_connection conn;
	conn.transport = &test_trsp_conn;
	conn.open = FIREFLY_CONNECTION_OPEN;
	conn.memory_replacements.alloc_replacement = NULL;
	conn.memory_replacements.free_replacement = NULL;

	// Construct decoder.
	struct labcomm_reader *r;
	r = transport_labcomm_reader_new(&conn, labcomm_default_memory);
	conn.transport_decoder =
			labcomm_decoder_new(r, NULL, labcomm_default_memory, NULL);
	if (conn.transport_decoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	// TODO: Fix when labcomm gets error handling back
	/* labcomm_register_error_handler_decoder(conn.transport_decoder, handle_labcomm_error);*/

	// Construct encoder.
	struct labcomm_writer *w;
	struct labcomm_encoder *test_enc;
	w = labcomm_static_buffer_writer_new();
	test_enc = labcomm_encoder_new(w, NULL, labcomm_default_memory, NULL);
	if (test_enc == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	// TODO: Fix when labcomm gets error handling back
	/* labcomm_register_error_handler_encoder(conn.transport_encoder, handle_labcomm_error);*/

	conn.event_queue = eq;

	unsigned char full_buf[512];
	size_t full_buf_size = 0;
	unsigned char *buf;
	size_t buf_size;
	int i;
	int res;
	labcomm_decoder_register_test_test_var(conn.transport_decoder,
			test_fragments_handle_test_var, &i);
	labcomm_encoder_register_test_test_var(test_enc);
	res = labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_RESET_BUFFER);
	CU_ASSERT_EQUAL_FATAL(res, 0);
	memcpy(full_buf + full_buf_size, buf, buf_size);
	full_buf_size += buf_size;
	free(buf);

	test_test_var v;
	for (i = 0; i < 19; i++) {
		v = i;
		labcomm_encode_test_test_var(test_enc, &v);
		res = labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
				&buf, &buf_size);
		labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_RESET_BUFFER);
		CU_ASSERT_EQUAL_FATAL(res, 0);
		memcpy(full_buf + full_buf_size, buf, buf_size);
		full_buf_size += buf_size;
		free(buf);
	}
	size_t frag_size = full_buf_size / 7;
	unsigned char *frags[7];
	for (i = 0; i < 6; i++) {
		frags[i] = malloc(frag_size);
		memcpy(frags[i], full_buf + i*frag_size, frag_size);
	}
	size_t last_frag_size = full_buf_size - 6*frag_size;
	frags[6] = malloc(last_frag_size);
	memcpy(frags[6], full_buf + 6*frag_size, last_frag_size);
	/*for (i = 0; i < 7; i++) { printf("frag %d: %p\n", i, frags[i]); }*/
	int dec_res = 0;
	for (i = 0; i < 6; i++) {
		/*printf("Adding buffer %d\n", i);*/
		labcomm_decoder_ioctl(conn.transport_decoder,
				FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER,
				frags[i], frag_size);
		while (dec_res >= 0) {
			/*printf("Decoding\n");*/
			dec_res = labcomm_decoder_decode_one(conn.transport_decoder);
		}
		dec_res = 0;
	}
	/*printf("Adding buffer %d\n", 6);*/
	labcomm_decoder_ioctl(conn.transport_decoder,
			FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER,
			frags[6], last_frag_size);
	while (dec_res >= 0) {
		/*printf("Decoding\n");*/
		dec_res = labcomm_decoder_decode_one(conn.transport_decoder);
	}
	CU_ASSERT_EQUAL(nbr_test_vars, 19);

	labcomm_encoder_free(test_enc);
	labcomm_decoder_free(conn.transport_decoder);
	successfully_decoded = false;
	nbr_test_vars = 0;
	firefly_event_queue_free(&eq);
}
