#include <stdio.h>

#include "CUnit/Basic.h"
#include "CUnit/Console.h"

#include "test/test_proto_translc.h"
#include "test/test_proto_protolc.h"
#include "test/test_proto_chan.h"
#include "test/test_transport_udp_posix.h"

int main()
{
	CU_pSuite translc_suite = NULL;
	CU_pSuite protolc_suite = NULL;
	CU_pSuite chan_suite = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	// Add our test suites.
	translc_suite = CU_add_suite("transport_enc_dec",
					init_suit_translc, clean_suit_translc);
	if (translc_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	protolc_suite = CU_add_suite("protocol_enc_dec",
					init_suit_protolc, clean_suit_protolc);
	if (protolc_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	chan_suite = CU_add_suite("chan_suite", init_suit_proto_chan, clean_suit_proto_chan);
	if (chan_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	// Transport encoding and decoding tests.
	if (
		(CU_add_test(translc_suite, "test_encode_protocol",
			     test_encode_protocol) == NULL)
		||
		(CU_add_test(translc_suite, "test_decode_protocol",
			     test_decode_protocol) == NULL)
		||
		(CU_add_test(translc_suite, "test_encode_decode_protocol",
			     test_encode_decode_protocol) == NULL)
		||
		(CU_add_test(translc_suite,
			     "test_decode_protocol_multiple_times",
			     test_decode_protocol_multiple_times) == NULL)
		||
		(CU_add_test(translc_suite,
			     "test_encode_protocol_multiple_times",
			     test_encode_protocol_multiple_times) == NULL)
	   ) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Protocol writer/reader tests.
	if (
		(CU_add_test(protolc_suite, "test_proto_writer",
			     test_proto_writer) == NULL)
		||
		(CU_add_test(protolc_suite, "test_proto_reader",
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
		(CU_add_test(chan_suite,
			     "test_next_channel_id",
			     test_next_channel_id) == NULL)
		||
		(CU_add_test(chan_suite, "test_chan_open",
			     test_chan_open) == NULL)
		||
		(CU_add_test(chan_suite, "test_chan_recv_accept",
			     test_chan_recv_accept) == NULL)
		||
		(CU_add_test(chan_suite, "test_chan_recv_reject",
			     test_chan_recv_reject) == NULL)
		||
		(CU_add_test(chan_suite, "test_chan_open_rejected",
			     test_chan_open_rejected) == NULL)
		||
		(CU_add_test(chan_suite, "test_chan_open_recv",
			     test_chan_open_recv) == NULL)
		||
	   (CU_add_test(chan_suite, "test_chan_close",
					test_chan_close) == NULL)
		||
	   (CU_add_test(chan_suite, "test_chan_recv_close",
					test_chan_recv_close) == NULL)
		||
	   (CU_add_test(chan_suite, "test_send_app_data",
					test_send_app_data) == NULL)
		||
	   (CU_add_test(chan_suite, "test_recv_app_data",
					test_recv_app_data) == NULL)
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
