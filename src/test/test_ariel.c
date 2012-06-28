
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "CUnit/Basic.h"
#include "CUnit/Console.h"

#include "gen/firefly_sample.h"
#include "ariel_protocol.h"

int init_suite()
{
	return 0; // Success.
}

int clean_suite()
{
	return 0; // Success.
}

void test_setup_simple()
{
	struct ariel_connection conn;
	size_t expected_beat_rate = 100;

	struct ff_result *actual_result;
	unsigned char *actual;

	char canary[] = "canary";
	int total_length = FIREFLY_SETUP_ARIEL_LEN + sizeof(canary);
	actual = (unsigned char *) malloc(total_length);
	memcpy(actual + FIREFLY_SETUP_ARIEL_LEN, canary, sizeof(canary));
	unsigned char expected[FIREFLY_SETUP_ARIEL_LEN];
	expected[0] = FIRELFY_SETUP_ARIEL;
	expected[1] = FIREFLY_DATA_TYPE_RAW;
	expected[2] = (unsigned char) ((expected_beat_rate >> 8) & 0xFF);
	expected[3] = (unsigned char) ((expected_beat_rate) & 0xFF);
	struct ff_result expected_result;
	expected_result.type = FIREFLY_RESULT_OK;
	actual_result = ariel_setup(&conn, actual, expected_beat_rate, FIREFLY_DATA_TYPE_RAW);

	CU_ASSERT_EQUAL(actual_result->type, expected_result.type);
	CU_ASSERT_EQUAL(conn.beat_rate, expected_beat_rate);
	CU_ASSERT_EQUAL(conn.type, FIREFLY_DATA_TYPE_RAW);
	CU_ASSERT_NSTRING_EQUAL(actual, expected, FIREFLY_SETUP_ARIEL_LEN);
	CU_ASSERT_NSTRING_EQUAL(actual+FIREFLY_SETUP_ARIEL_LEN, canary, sizeof(canary));
	
	free(actual_result);
	free(actual);
	
}

void test_setup_with_null_buffer() {
	struct ariel_connection conn;
	struct ff_result *actual_result;
	unsigned char *buffer;

	buffer = NULL;

	actual_result = ariel_setup(&conn, buffer, 100, FIREFLY_DATA_TYPE_RAW);

	CU_ASSERT_EQUAL(actual_result->type, FIREFLY_NULL_BUFFER);

	free(actual_result);
}

void test_setup_invalid_beat_rate()
{
	unsigned char buffer[4];
	size_t invalid_beat_rate_high = 65536;
	size_t invalid_beat_rate_low = 0;
	struct ariel_connection conn;

	struct ff_result *res = ariel_setup(&conn, buffer, invalid_beat_rate_high, FIREFLY_DATA_TYPE_RAW);
	CU_ASSERT_EQUAL(res->type, FIREFLY_INVALID_BEAT_RATE);
	free(res);

	res = ariel_setup(&conn, buffer, invalid_beat_rate_low, FIREFLY_DATA_TYPE_RAW);
	CU_ASSERT_EQUAL(res->type, FIREFLY_INVALID_BEAT_RATE);
	free(res);
}

static bool successfully_decoded_data = false;

static void handle_firefly_data(firefly_sample_data  *samp, void *context)
{
	successfully_decoded_data = true;
}

void test_setup_parse_simple()
{
	unsigned char buffer[] = {
		FIREFLY_SETUP_SERENITY,
		0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x80,
	       	0x00, 0x00, 0x00, 0x04, 0x64, 0x61, 0x74, 0x61,
	       	0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01,
	       	0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x22
	};
	unsigned char labcomm_data[] = {
		0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x01,
	       	0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05
	};
	struct ariel_connection conn;
	conn.beat_rate = 100;
	conn.type = FIREFLY_DATA_TYPE_RAW;

	struct ff_result *res = ariel_setup_parse(&conn, buffer);
	CU_ASSERT_EQUAL(res->type, FIREFLY_RESULT_OK);

	conn.decoder_mcontext->enc_data = labcomm_data;
	labcomm_decoder_register_firefly_sample_data(conn.decoder, handle_firefly_data, NULL);
	conn.decoder_mcontext->size = sizeof(labcomm_data);

	labcomm_decoder_decode_one(conn.decoder);

	CU_ASSERT_TRUE(successfully_decoded_data);

}

int main()
{
	CU_pSuite pSuite = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	// Add our test suites.
	pSuite = CU_add_suite("Suite Setup", init_suite, clean_suite);
	if (pSuite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	
	if (
		(CU_add_test(pSuite, "test_setup_simple", test_setup_simple) == NULL) ||
		(CU_add_test(pSuite, "test_setup_with_null_buffer", test_setup_with_null_buffer) == NULL) ||
		(CU_add_test(pSuite, "test_setup_invalid_beat_rate", test_setup_invalid_beat_rate) == NULL)
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
