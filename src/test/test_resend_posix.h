#ifndef TEST_RESEND_POSIX_H
#define TEST_RESEND_POSIX_H

int setup_resend_posix();
int teardown_resend_posix();

void test_add_simple();
void test_remove_simple();
void test_remove_non_existing_elem();
void test_top_simple();

#endif
