/**
 * @file
 * @brief Test the functionality of the protocol layer.
 */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include <labcomm_mem_writer.h>

#include <gen/firefly_protocol.h>
#include <firefly_errors.h>
#include "protocol/firefly_protocol_private.h"
#include <protocol/firefly_protocol.h>

#define WRITE_BUF_SIZE 64	// Size of the labcomm buffer to write to.
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
static struct connection the_conn;

void handle_firefly_protocol_proto(firefly_protocol_proto *proto, void *context)	// <-- skillz
{
	struct connection *conn = (struct connection *) context;
	CU_ASSERT_PTR_EQUAL(conn, &the_conn);

	CU_ASSERT_EQUAL(proto->chan_id, chan_id);
	CU_ASSERT_EQUAL(proto->important, important);
	CU_ASSERT_EQUAL(proto->app_enc_data.n_0, sizeof(app_data));
	CU_ASSERT_EQUAL(0, memcmp(proto->app_enc_data.a, app_data,
						sizeof(app_data)));
}

// Simulate that the data is sent over the net an arrives the same way
// on the other end. Now we decoce it and hope it's the same stuff we
// previously encoded.
void transport_write_udp_posix_mock(unsigned char *data, size_t data_size,
		struct connection *conn)
{
	unsigned char *zero_buf = calloc(1, WRITE_BUF_SIZE);
	// The buffer shoule not be empty anymore.
	CU_ASSERT_NOT_EQUAL(0, memcmp(data, zero_buf, WRITE_BUF_SIZE));

	// Construct decoder.
	struct labcomm_decoder *decoder = labcomm_decoder_new(
			ff_transport_reader, conn);
	labcomm_register_error_handler_decoder(decoder,
		       	labcomm_error_to_ff_error);
	labcomm_decoder_register_firefly_protocol_proto(decoder,
		       	handle_firefly_protocol_proto, conn); // TODO meaning
								// to pass
								// conn?

}

void test_encode_decode_protocol()
{
	struct ff_transport_writer_data writer_data;
	writer_data.data = (unsigned char *) calloc(1, WRITE_BUF_SIZE);
	writer_data.data_size = WRITE_BUF_SIZE;
	writer_data.pos = 0;

	the_conn.transport_conn_platspec = NULL;
	the_conn.writer_data = &writer_data;
	the_conn.transport_write = transport_write_udp_posix_mock;

	struct labcomm_encoder *encoder = labcomm_encoder_new(
			ff_transport_writer, &the_conn);
	labcomm_register_error_handler_encoder(encoder,
		       	labcomm_error_to_ff_error);
	labcomm_encoder_register_firefly_protocol_proto(encoder);

	firefly_protocol_proto proto;
	proto.chan_id = chan_id;
	proto.important = important;
	proto.app_enc_data.n_0 = sizeof(app_data);
	proto.app_enc_data.a = app_data;
	labcomm_encode_firefly_protocol_proto(encoder, &proto);
}

int main()
{
	CU_pSuite pSuite = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	// Add our test suites.
	pSuite = CU_add_suite("basic", init_suit, clean_suit);
	if (pSuite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	if (
		(CU_add_test(pSuite, "test_encode_decode_protocol",
			     test_encode_decode_protocol) == NULL)
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
