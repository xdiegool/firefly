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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <gen/firefly_protocol.h>
#include <gen/test.h>
#include <utils/firefly_errors.h>
#include <protocol/firefly_protocol.h>

#include "protocol/firefly_protocol_private.h"
#include "test/test_labcomm_utils.h"
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
	labcomm_decoder_ioctl(conn->transport_decoder,
			FIREFLY_LABCOMM_IOCTL_READER_SET_BUFFER, data, data_size);
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

	// Construct decoder.
	struct labcomm_decoder *decoder =
		labcomm_decoder_new(transport_labcomm_reader_new(&conn), NULL);
	if (decoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_decoder(decoder, handle_labcomm_error);
	conn.transport_decoder = decoder;
	// Construct encoder.
	struct labcomm_encoder *encoder =
		labcomm_encoder_new(transport_labcomm_writer_new(&conn), NULL);
	if (encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_encoder(encoder, handle_labcomm_error);
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
	conn.transport_decoder =
		labcomm_decoder_new(transport_labcomm_reader_new(&conn), NULL);
	if (conn.transport_decoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_decoder(conn.transport_decoder, handle_labcomm_error);
	// Construct encoder.
	conn.transport_encoder =
		labcomm_encoder_new(transport_labcomm_writer_new(&conn), NULL);
	if (conn.transport_encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_encoder(conn.transport_encoder, handle_labcomm_error);

	conn.event_queue = eq;

	struct firefly_channel chan = {0};

	chan.proto_decoder =
		labcomm_decoder_new(protocol_labcomm_reader_new(&conn), NULL);
	chan.proto_encoder =
		labcomm_encoder_new(protocol_labcomm_writer_new(&chan), NULL);
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
