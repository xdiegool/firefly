#ifndef TEST_PROTO_CHAN_H
#define TEST_PROTO_CHAN_H

int init_suit_proto_chan();

int clean_suit_proto_chan();

/* Test utilities */
void test_next_channel_id();
void test_get_conn();

/* Test handshake */
void test_chan_open();
void test_chan_open_rejected();
void test_chan_recv_accept();
void test_chan_recv_reject();
void test_chan_open_recv();
void test_chan_close();
void test_chan_recv_close();

/* Test restrict */
void test_restrict_recv();
void test_restrict_recv_reject();
void test_restrict_recv_dup();
void test_unrestrict_recv();
void test_unrestrict_recv_dup();
void test_restrict_req();

/* Test data exchange */
void test_send_app_data();
void test_recv_app_data();
void test_transmit_app_data_over_mock_trans_layer();
void test_chan_open_close_multiple();
void test_chan_app_data_multiple();
void test_nbr_chan();

#endif
