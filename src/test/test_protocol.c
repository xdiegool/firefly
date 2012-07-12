/**
 * @file
 * @brief Test the functionality of the protocol layer.
 */
#include <stdio.h>
#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include <labcomm_mem_writer.h>

#include <gen/firefly_protocol.h>
#include <firefly_errors.h>
#include "protocol/firefly_protocol_private.h"

int init_suit()
{
	return 0; // Success.
}

int clean_suit()
{
	return 0; // Success.
}


void test_encode_protocol()
{
	unsigned char *write_buf = malloc(64);
	labcomm_mem_writer_context_t *mcontext =
			labcomm_mem_writer_context_t_new(0, 64, write_buf);
	// TODO don't use mem_writer but an own transport jwriter.
	struct labcomm_encoder *encoder = labcomm_encoder_new(labcomm_mem_writer, mcontext);
	labcomm_register_error_handler_encoder(encoder, labcomm_error_to_ff_error);
	labcomm_encoder_register_firefly_protocol_proto(encoder);

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
		(CU_add_test(pSuite, "test_encode_protocol", test_encode_protocol) == NULL)
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
