
#ifndef TEST_TRANSPORT_UDP_POSIX_H
#define TEST_TRANSPORT_UDP_POSIX_H

int init_suit_udp_posix();

int clean_suit_udp_posix();

void test_recv_connection();
void test_recv_data();
void test_recv_conn_and_data();
void test_recv_conn_keep();
void test_recv_conn_reject();
void test_recv_conn_keep_two();
void test_recv_conn_and_two_data();
void test_conn_open_and_send();
void test_conn_open_and_recv();

// test free llp
void test_llp_free_empty();
void test_llp_free_mult_conns();
void test_llp_free_mult_conns_w_chans();

void test_open_and_recv_with_two_llp();
void test_recv_big_connection();

void test_read_mult_threads();

// test resend buffer
void test_send_important();
void test_send_important_ack();
void test_send_important_id_null();
void test_send_important_long_timeout();

#endif
