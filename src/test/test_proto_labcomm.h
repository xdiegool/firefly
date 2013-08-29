#ifndef TEST_PROTO_LABCOMM_H
#define TEST_PROTO_LABCOMM_H

int init_suit_labcomm();
int clean_suit_labcomm();

void test_encode_decode_protocol();
void test_encode_decode_app();
void test_decode_large_protocol_fragments();
void test_decode_small_protocol_fragments();

#endif
