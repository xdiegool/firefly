/*#include "test/test_transport.h"*/

#include <stdlib.h>
#include <stdbool.h>
#include "utils/cppmacros.h"

#include "CUnit/Basic.h"

#include <protocol/firefly_protocol.h>

unsigned char send_buf[] = {0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

bool data_received = false;
size_t data_recv_size;
unsigned char *data_recv_buf;

void protocol_data_received(struct firefly_connection *conn, unsigned char *data,
							size_t size)
{
	UNUSED_VAR(conn);
	if (!data_recv_buf) {
		data_recv_buf = send_buf;
		data_recv_size = sizeof(send_buf);
	}
	CU_ASSERT_EQUAL(data_recv_size, size);
	CU_ASSERT_NSTRING_EQUAL(data, data_recv_buf, size);
	data_received = true;
}
