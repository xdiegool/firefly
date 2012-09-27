#ifndef TEST_TRANSPORT_ETH_POSIX_H
#define TEST_TRANSPORT_ETH_POSIX_H

int init_suit_eth_posix();

int clean_suit_eth_posix();

//void test_eth_find_conn_by_address()
void test_eth_recv_connection();
void test_eth_recv_data();
void test_eth_recv_conn_and_data();
void test_eth_recv_conn_keep();
void test_eth_recv_conn_reject();
void test_eth_recv_conn_and_two_data();
void test_eth_conn_open_and_send();
void test_eth_conn_open_and_recv();
void test_eth_recv_conn_keep_two();
void test_eth_recv_data_two_conn();

// TODO test close connection
// TODO test real ethernet

void test_eth_read();

/*
void test_find_conn_by_addr();
void test_cleanup_simple();
void test_cleanup_many_conn();
void test_add_conn_to_llp();
void test_null_pointer_as_callback();
void test_open_and_recv_with_two_llp();
void test_recv_big_connection();
void test_reader_scale_back();

void test_read_mult_threads();
*/

#endif
