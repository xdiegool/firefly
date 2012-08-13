#ifndef TEST_PROTO_CHAN_H
#define TEST_PROTO_CHAN_H

int init_suit_proto_chan();

int clean_suit_proto_chan();

void test_next_channel_id();

void test_get_streams();

void test_get_conn();

void test_chan_open();

void test_chan_open_rejected();

void test_chan_recv_accept();

void test_chan_recv_reject();

void test_chan_open_recv();

void test_chan_close();

void test_chan_recv_close();

void test_send_app_data();

void test_recv_app_data();

void test_transmit_app_data_over_mock_trans_layer();

void test_chan_open_close_multiple();

void test_chan_app_data_multiple();

// TODO
// multiple channels
// importance?

#endif
