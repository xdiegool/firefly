#ifndef TEST_PROTO_RESEND_H
#define TEST_PROTO_RESEND_H

int init_suit_proto_important();
int clean_suit_proto_important();

void test_important_signature();
void test_important_recv_ack();
void test_important_signatures_mult();
void test_important_seqno_overflow();
void test_important_send_ack();
void test_not_important_not_send_ack();

#endif
