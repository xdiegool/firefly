#include "CUnit/Basic.h"
#include "CUnit/Console.h"

#include "test/test_transport_udp_posix.h"
#include "test/test_transport_eth_posix.h"

int main()
{
	CU_pSuite trans_udp_posix = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	trans_udp_posix = CU_add_suite("udp_core", init_suit_udp_posix, clean_suit_udp_posix);
	if (trans_udp_posix == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	 /*Transport UDP Posix tests.*/
	if (
		(CU_add_test(trans_udp_posix, "test_find_conn_by_addr",
				test_find_conn_by_addr) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_cleanup_simple",
				test_cleanup_simple) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_cleanup_many_conn",
				test_cleanup_many_conn) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_add_conn_to_llp",
				test_add_conn_to_llp) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_recv_connection",
				test_recv_connection) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_recv_data",
				test_recv_data) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_recv_conn_and_data",
				test_recv_conn_and_data) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_recv_conn_keep",
				test_recv_conn_keep) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_recv_conn_reject",
				test_recv_conn_reject) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_recv_conn_keep_two",
				test_recv_conn_keep_two) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_recv_conn_and_two_data",
				test_recv_conn_and_two_data) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_null_pointer_as_callback",
				test_null_pointer_as_callback) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_conn_open_and_send",
				test_conn_open_and_send) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_conn_open_and_recv",
				test_conn_open_and_recv) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_open_and_recv_with_two_llp",
				test_open_and_recv_with_two_llp) == NULL)
				||
		(CU_add_test(trans_udp_posix, "test_recv_big_connection",
					 test_recv_big_connection) == NULL)
				||
		(CU_add_test(trans_udp_posix, "test_reader_scale_back",
					 test_reader_scale_back) == NULL)
				||
		(CU_add_test(trans_udp_posix, "test_read_mult_threads",
					 test_read_mult_threads) == NULL)
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
