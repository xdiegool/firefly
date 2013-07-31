#include "CUnit/Basic.h"
#include "CUnit/Console.h"

#include "test/test_transport_udp_posix.h"
#include "test/test_transport_gen.h"

int main()
{
	CU_pSuite trans_udp_posix = NULL;
	CU_pSuite trans_gen = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	trans_udp_posix = CU_add_suite("udp_core", init_suit_udp_posix, clean_suit_udp_posix);
	trans_gen = CU_add_suite("general", init_suit_general, clean_suit_general);
	if (trans_udp_posix == NULL || trans_gen == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/*Transport general tests.*/
	if (
		(CU_add_test(trans_gen, "test_find_conn_by_addr",
				test_find_conn_by_addr) == NULL)
			   ||
		(CU_add_test(trans_gen, "test_add_conn_to_llp",
				test_add_conn_to_llp) == NULL)
			   ||
		(CU_add_test(trans_gen, "test_remove_conn_by_addr",
				test_remove_conn_by_addr) == NULL)
	) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/*Transport UDP Posix tests.*/
	if (
		(CU_add_test(trans_udp_posix, "test_recv_connection",
				test_recv_connection) == NULL)
			   ||
		(CU_add_test(trans_udp_posix, "test_recv_conn_null_cb",
				test_recv_conn_null_cb) == NULL)
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
		(CU_add_test(trans_udp_posix, "test_read_mult_threads",
					 test_read_mult_threads) == NULL)
				||
		(CU_add_test(trans_udp_posix, "test_llp_free_empty",
					 test_llp_free_empty) == NULL)
				||
		(CU_add_test(trans_udp_posix, "test_llp_free_mult_conns",
					 test_llp_free_mult_conns) == NULL)
				||
		(CU_add_test(trans_udp_posix, "test_llp_free_mult_conns_w_chans",
					 test_llp_free_mult_conns_w_chans) == NULL)
				||
		(CU_add_test(trans_udp_posix, "test_send_important",
					 test_send_important) == NULL)
				||
		(CU_add_test(trans_udp_posix, "test_send_important_ack",
					 test_send_important_ack) == NULL)
				||
		(CU_add_test(trans_udp_posix, "test_send_important_id_null",
					 test_send_important_id_null) == NULL)
				||
		(CU_add_test(trans_udp_posix, "test_send_important_long_timeout",
					 test_send_important_long_timeout) == NULL)
	   ) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Set verbosity.
	CU_basic_set_mode(CU_BRM_VERBOSE);
	/*CU_console_run_tests();*/

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
