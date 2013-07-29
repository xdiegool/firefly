/**
 * @file
 * @brief Test the protocol layer.
 */

#include "CUnit/Basic.h"
#include "CUnit/Console.h"

#include "test/test_proto_labcomm.h"
#include "test/test_proto_chan.h"
#include "test/test_proto_conn.h"
#include "test/test_proto_important.h"
#include "test/test_proto_errors.h"
#include "test/test_transport_udp_posix.h"

int main()
{
	CU_pSuite labcomm_suite = NULL;
	CU_pSuite chan_suite = NULL;
	CU_pSuite conn_suite = NULL;
	CU_pSuite important_suite = NULL;
	CU_pSuite errors_suite = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	// Add our test suites.
	labcomm_suite = CU_add_suite("transport_enc_dec",
			init_suit_labcomm, clean_suit_labcomm);
	if (labcomm_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	chan_suite = CU_add_suite("chan_suite", init_suit_proto_chan,
					clean_suit_proto_chan);
	if (chan_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	conn_suite = CU_add_suite("conn_suite", init_suit_proto_conn,
					clean_suit_proto_conn);
	if (conn_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	important_suite = CU_add_suite("important_suite", init_suit_proto_important,
					clean_suit_proto_important);
	if (important_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	errors_suite = CU_add_suite("errors_suite", init_suite_proto_errors,
					clean_suite_proto_errors);
	if (errors_suite == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Transport encoding and decoding tests.
	if (
			(CU_add_test(labcomm_suite,
					"test_encode_decode_protocol",
					test_encode_decode_protocol) == NULL)
			||
			(CU_add_test(labcomm_suite,
					"test_encode_decode_app",
					test_encode_decode_app) == NULL)
		) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Connection tests.
	if (
			(CU_add_test(conn_suite, "test_conn_close_empty",
					test_conn_close_empty) == NULL)
			||
			(CU_add_test(conn_suite, "test_conn_close_open_chan",
					test_conn_close_open_chan) == NULL)
			||
			(CU_add_test(conn_suite, "test_conn_close_send_data",
					test_conn_close_send_data) == NULL)
			||
			(CU_add_test(conn_suite, "test_conn_close_send_first",
					test_conn_close_send_first) == NULL)
			||
			(CU_add_test(conn_suite, "test_conn_close_mult_chans",
					test_conn_close_mult_chans) == NULL)
			||
			(CU_add_test(conn_suite, "test_conn_close_mult_chans_overflow",
					test_conn_close_mult_chans_overflow) == NULL)
			||
			(CU_add_test(conn_suite, "test_conn_close_recv_any",
					test_conn_close_recv_any) == NULL)
			||
			(CU_add_test(conn_suite, "test_conn_close_recv_chan_req_first",
					test_conn_close_recv_chan_req_first) == NULL)
		) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/*// Channel tests.*/
	if (
			(CU_add_test(chan_suite, "test_next_channel_id",
					test_next_channel_id) == NULL)
			||
			(CU_add_test(chan_suite, "test_get_conn",
					test_get_conn) == NULL)
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
			||
			(CU_add_test(chan_suite,
					"test_transmit_app_data_over_mock_trans_layer",
					test_transmit_app_data_over_mock_trans_layer)
					== NULL)
			||
			(CU_add_test(chan_suite, "test_chan_open_close_multiple",
					test_chan_open_close_multiple) == NULL)
			) {
				CU_cleanup_registry();
				return CU_get_error();
			}
	/*// Protocol important tests.*/
	if (
			(CU_add_test(important_suite, "test_important_signature",
					test_important_signature) == NULL)
			||
			(CU_add_test(important_suite, "test_important_recv_ack",
					test_important_recv_ack) == NULL)
			||
			(CU_add_test(important_suite, "test_important_signatures_mult",
					test_important_signatures_mult) == NULL)
			||
			(CU_add_test(important_suite, "test_important_seqno_overflow",
					test_important_seqno_overflow) == NULL)
			||
			(CU_add_test(important_suite, "test_important_send_ack",
					test_important_send_ack) == NULL)
			||
			(CU_add_test(important_suite, "test_not_important_not_send_ack",
					test_not_important_not_send_ack) == NULL)
			||
			(CU_add_test(important_suite, "test_errorneous_ack",
					test_errorneous_ack) == NULL)
			||
			(CU_add_test(important_suite, "test_important_mult_simultaneously",
					test_important_mult_simultaneously) == NULL)
			||
			(CU_add_test(important_suite, "test_important_recv_duplicate",
					test_important_recv_duplicate) == NULL)
			||
			(CU_add_test(important_suite, "test_important_handshake_recv",
					test_important_handshake_recv) == NULL)
			||
			(CU_add_test(important_suite, "test_important_handshake_recv_errors",
					test_important_handshake_recv_errors) == NULL)
			||
			(CU_add_test(important_suite, "test_important_handshake_open",
					test_important_handshake_open) == NULL)
			||
			(CU_add_test(important_suite, "test_important_handshake_open_errors",
					test_important_handshake_open_errors) == NULL)
			||
			(CU_add_test(important_suite, "test_important_ack_on_close",
					test_important_ack_on_close) == NULL)
		) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/*// Errors tests.*/
	if (
			(CU_add_test(chan_suite, "test_unexpected_ack",
					test_unexpected_ack) == NULL)
			||
			(CU_add_test(chan_suite, "test_unexpected_response",
					test_unexpected_response) == NULL)
			||
			(CU_add_test(chan_suite, "test_unexpected_data_sample",
					test_unexpected_data_sample) == NULL)
			||
			(CU_add_test(chan_suite, "test_unexpected_channel_close",
					test_unexpected_channel_close) == NULL)
			||
			(CU_add_test(chan_suite, "test_nbr_chan",
					test_nbr_chan) == NULL)
			) {
				CU_cleanup_registry();
				return CU_get_error();
			}

	// Set verbosity.
	CU_basic_set_mode(CU_BRM_VERBOSE);

	// Run all test suites.
	CU_basic_run_tests();
	int res = CU_get_number_of_tests_failed();
	// Clean up.
	CU_cleanup_registry();

	if (res != 0) {
		return 1;
	}
	return CU_get_error();
}
