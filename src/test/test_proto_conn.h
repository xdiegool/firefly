#ifndef TEST_PROTO_CONN_H
#define TEST_PROTO_CONN_H

int init_suit_proto_conn();

int clean_suit_proto_conn();

void test_conn_close_empty();
void test_conn_close_open_chan();
void test_conn_close_send_data();
void test_conn_close_send_first();
void test_conn_close_mult_chans();

#endif
