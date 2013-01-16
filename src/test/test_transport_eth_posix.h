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
void test_eth_recv_conn_keep_two();
void test_eth_recv_conn_and_two_data();
void test_eth_conn_open_and_send();
void test_eth_conn_open_and_recv();
void test_eth_recv_data_two_conn();

void test_eth_read();

// test free llp
void test_eth_llp_free_empty();
void test_eth_llp_free_mult_conns();
void test_eth_llp_free_mult_conns_w_chans();

/*
void test_read_mult_threads();
*/

#endif
