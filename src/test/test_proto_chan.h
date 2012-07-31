
#ifndef TEST_PROTO_CHAN_H
#define TEST_PROTO_CHAN_H

int init_suit_proto_chan();

int clean_suit_proto_chan();

void test_get_streams();

void test_chan_open();

void test_chan_open_rejected();

void test_chan_recv_accept();

void test_chan_recv_reject();

void test_chan_open_recv();

void test_chan_close();

void test_chan_recv_close();

// TODO
// send app data
// recv app data
// importance?

#endif
