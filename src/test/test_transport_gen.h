
#include <stdbool.h>

#include "CUnit/Basic.h"
#include "CUnit/Console.h"

#include <transport/firefly_transport.h>
#include "transport/firefly_transport_private.h"

int init_suit_general();
int clean_suit_general();

void test_add_conn_to_llp();
void test_remove_conn_by_addr();
void test_find_conn_by_addr();
