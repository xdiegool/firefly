
#ifndef TEST_TRANSPORT_UDP_POSIX_H
#define TEST_TRANSPORT_UDP_POSIX_H

int init_suit_udp_posix();

int clean_suit_udp_posix();

void test_cleanup_simple();
void test_cleanup_many_conn();

void test_recv_connection();
void test_recv_data();
void test_recv_conn_and_data();
void test_recv_conn_keep();
void test_recv_conn_reject();
void test_recv_conn_keep_two();
void test_recv_conn_and_two_data();
void test_conn_open_and_send();
void test_conn_open_and_recv();

void test_open_and_recv_with_two_llp();
void test_recv_big_connection();
void test_reader_scale_back();
void test_null_pointer_as_callback();

void test_read_mult_threads();

#endif
