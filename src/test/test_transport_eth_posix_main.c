
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "CUnit/Basic.h"
#include "CUnit/Console.h"

#include "test/test_transport_eth_posix.h"

int main()
{
	uid_t uid;
	uid = geteuid();
	if (uid != 0) {
		fprintf(stderr, "Need root to run these tests\n");
		return 0;
	}
	CU_pSuite trans_eth_posix = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	trans_eth_posix = CU_add_suite("eth_core", init_suit_eth_posix, clean_suit_eth_posix);
	if (trans_eth_posix == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	if (
		(CU_add_test(trans_eth_posix, "test_eth_recv_connection",
				test_eth_recv_connection) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_data",
				test_eth_recv_data) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_conn_and_data",
				test_eth_recv_conn_and_data) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_conn_keep",
				test_eth_recv_conn_keep) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_conn_reject",
				test_eth_recv_conn_reject) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_conn_and_two_data",
				test_eth_recv_conn_and_two_data) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_conn_open_and_send",
				test_eth_conn_open_and_send) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_conn_open_and_recv",
				test_eth_conn_open_and_recv) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_conn_keep_two",
				test_eth_recv_conn_keep_two) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_recv_data_two_conn",
				test_eth_recv_data_two_conn) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_llp_free_empty",
				test_eth_llp_free_empty) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_llp_free_mult_conns",
				test_eth_llp_free_mult_conns) == NULL)
			   ||
		(CU_add_test(trans_eth_posix, "test_eth_llp_free_mult_conns_w_chans",
				test_eth_llp_free_mult_conns_w_chans) == NULL)
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
