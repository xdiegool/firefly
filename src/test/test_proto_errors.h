#ifndef TEST_PROTO_ERRORS_H
#define TEST_PROTO_ERRORS_H

int init_suite_proto_errors();

int clean_suite_proto_errors();

void test_unexpected_ack();

void test_unexpected_response();

void test_unexpected_data_sample();

#endif
