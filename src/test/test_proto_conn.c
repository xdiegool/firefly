#include "test/test_proto_chan.h"

#include "CUnit/Basic.h"
#include <gen/test.h>

#include <stdbool.h>

#include "protocol/firefly_protocol_private.h"
#include <utils/firefly_errors.h>

int init_suit_proto_conn()
{
	return 0;
}

int clean_suit_proto_conn()
{
	return 0;
}

extern bool was_in_error;
extern enum firefly_error expected_error;

static bool plat_freed = false;

void free_plat_conn_test(struct firefly_connection *conn)
{
	free(conn->transport_conn_platspec);
	plat_freed = true;
}

void test_conn_close_empty()
{
	char *dummy_data = malloc(5);
	for (int i = 0; i < 5; i++) {
		dummy_data[i] = i;
	}
	struct firefly_event *ev;

	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	struct firefly_connection *conn = firefly_connection_new_register(
			NULL, NULL, NULL,
			NULL, eq, dummy_data, free_plat_conn_test, false);

	CU_ASSERT_EQUAL(conn->open, FIREFLY_CONNECTION_OPEN);
	firefly_connection_close(conn);
	CU_ASSERT_EQUAL(conn->open, FIREFLY_CONNECTION_CLOSED);

	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	CU_ASSERT_TRUE(plat_freed);

	firefly_event_queue_free(&eq);
	plat_freed = false;
}

void test_conn_close_open_chan()
{

	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	struct firefly_connection *conn = firefly_connection_new_register(
			NULL, NULL, NULL,
			NULL, eq, NULL, NULL, false);

	firefly_connection_close(conn);
	struct firefly_event *close_ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(close_ev);

	expected_error = FIREFLY_ERROR_PROTO_STATE;
	firefly_channel_open(conn, NULL);
	CU_ASSERT_TRUE(was_in_error);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NULL(ev);

	firefly_event_execute(close_ev);

	was_in_error = false;
	firefly_event_queue_free(&eq);
}

void test_conn_close_send_data()
{

	struct firefly_event_queue *eq = firefly_event_queue_new(
			firefly_event_add, NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	struct firefly_connection *conn = firefly_connection_new_register(
			NULL, NULL, NULL,
			NULL, eq, NULL, NULL, false);

	struct firefly_channel *ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	firefly_connection_close(conn);
	struct firefly_event *close_ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(close_ev);

	expected_error = FIREFLY_ERROR_PROTO_STATE;
	struct labcomm_encoder *ch_enc = firefly_protocol_get_output_stream(ch);
	labcomm_encoder_register_test_test_var(ch_enc);

	CU_ASSERT_TRUE(was_in_error);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NULL(ev);

	firefly_channel_closed_event(ch);
	firefly_event_execute(close_ev);

	was_in_error = false;
	firefly_event_queue_free(&eq);
}
