#include "test/test_proto_chan.h"

#include "CUnit/Basic.h"
#include <gen/test.h>
#include <gen/firefly_protocol.h>

#include <stdbool.h>
#include <labcomm.h>
#include <labcomm_ioctl.h>
#include <labcomm_static_buffer_writer.h>

#include "test/test_labcomm_utils.h"
#include "test/proto_helper.h"
#include "test/event_helper.h"
#include "protocol/firefly_protocol_private.h"
#include <utils/firefly_errors.h>
#include "utils/cppmacros.h"

static struct firefly_event_queue *eq = NULL;
extern unsigned int nbr_added_events;
extern int64_t test_event_ids[50];
extern int64_t test_event_deps[50][FIREFLY_EVENT_QUEUE_MAX_DEPENDS];

extern struct labcomm_encoder *test_enc;

int init_suit_proto_conn()
{
	init_labcomm_test_enc_dec();
	eq = firefly_event_queue_new(mock_test_event_add, 14, NULL);
	return 0;
}

int clean_suit_proto_conn()
{
	clean_labcomm_test_enc_dec();
	mock_test_event_queue_reset(eq);
	firefly_event_queue_free(&eq);
	return 0;
}

extern bool was_in_error;
extern enum firefly_error expected_error;

static bool plat_freed = false;

static int free_plat_conn_test(struct firefly_connection *conn)
{
	UNUSED_VAR(conn);
	plat_freed = true;
	return 0;
}

static bool transport_sent = false;
static void transport_write_mock(unsigned char *data, size_t size,
		struct firefly_connection *conn, bool important, unsigned char *id)
{
	UNUSED_VAR(data);
	UNUSED_VAR(size);
	UNUSED_VAR(conn);
	UNUSED_VAR(important);
	UNUSED_VAR(id);
	transport_sent = true;
}

static int test_conn_open(struct firefly_connection *conn)
{
	struct firefly_connection **res = conn->transport->context;
	*res = conn;
	return 0;
}

void test_conn_close_empty()
{
	struct firefly_connection *conn;
	struct firefly_transport_connection test_trsp_conn = {
		.write = NULL,
		.ack = NULL,
		.open = test_conn_open,
		.close = free_plat_conn_test,
		.context = &conn
	};
	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	CU_ASSERT_EQUAL(conn->open, FIREFLY_CONNECTION_OPEN);
	firefly_connection_close(conn);

	event_execute_test(eq, 2);
	CU_ASSERT_TRUE(plat_freed);

	plat_freed = false;
	transport_sent = false;
}

void test_conn_close_mult_chans()
{
	struct firefly_connection *conn;
	struct firefly_transport_connection test_trsp_conn = {
		.write = transport_write_mock,
		.ack = NULL,
		.open = test_conn_open,
		.close = free_plat_conn_test,
		.context = &conn
	};
	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	struct firefly_channel *ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	ch = firefly_channel_new(conn);
	ch->remote_id = 1;
	add_channel_to_connection(ch, conn);

	ch = firefly_channel_new(conn);
	ch->remote_id = 2;
	add_channel_to_connection(ch, conn);

	firefly_connection_close(conn);
	CU_ASSERT_FALSE(plat_freed);
	event_execute_test(eq, 7);
	CU_ASSERT_FALSE(plat_freed);
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(plat_freed);

	transport_sent = false;
	plat_freed = false;
}

void test_conn_close_mult_chans_overflow()
{
	struct firefly_connection *conn;
	struct firefly_transport_connection test_trsp_conn = {
		.write = transport_write_mock,
		.ack = NULL,
		.open = test_conn_open,
		.close = free_plat_conn_test,
		.context = &conn
	};
	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	for (int i = 0; i < FIREFLY_EVENT_QUEUE_MAX_DEPENDS + 1; i++) {
		struct firefly_channel *ch = firefly_channel_new(conn);
		ch->remote_id = i;
		add_channel_to_connection(ch, conn);
	}


	firefly_connection_close(conn);
	CU_ASSERT_FALSE(plat_freed);

	mock_test_event_queue_reset(eq);
	event_execute_test(eq, 1);

	CU_ASSERT_EQUAL(nbr_added_events, 1 + FIREFLY_EVENT_QUEUE_MAX_DEPENDS * 2);
	for (int i = 0; i < FIREFLY_EVENT_QUEUE_MAX_DEPENDS; i++) {
		CU_ASSERT_EQUAL(test_event_ids[i*2+1],
				test_event_deps[FIREFLY_EVENT_QUEUE_MAX_DEPENDS*2][i]);
	}

	mock_test_event_queue_reset(eq);
	event_execute_test(eq, 1 + FIREFLY_EVENT_QUEUE_MAX_DEPENDS * 2);
	CU_ASSERT_FALSE(plat_freed);
	CU_ASSERT_EQUAL(nbr_added_events, 3);
	CU_ASSERT_EQUAL(test_event_ids[1], test_event_deps[2][0]);

	event_execute_test(eq, 2);
	CU_ASSERT_FALSE(plat_freed);
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(plat_freed);

	transport_sent = false;
}

void test_conn_close_open_chan()
{
	struct firefly_connection *conn;
	struct firefly_transport_connection test_trsp_conn = {
		.write = transport_write_mock,
		.ack = NULL,
		.open = test_conn_open,
		.close = NULL,
		.context = &conn
	};
	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	struct firefly_channel *ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	firefly_connection_close(conn);
	event_execute_test(eq, 3);
	struct firefly_event *close_ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL_FATAL(close_ev);

	expected_error = FIREFLY_ERROR_PROTO_STATE;
	firefly_channel_open(conn);
	CU_ASSERT_TRUE(was_in_error);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NULL(ev);

	firefly_event_execute(close_ev);
	firefly_event_return(eq, &close_ev);

	was_in_error = false;
	transport_sent = false;
	expected_error = FIREFLY_ERROR_FIRST;
}

void test_conn_close_send_data()
{
	struct firefly_connection *conn;
	struct firefly_transport_connection test_trsp_conn = {
		.write = transport_write_mock,
		.ack = NULL,
		.open = test_conn_open,
		.close = NULL,
		.context = &conn
	};
	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	struct firefly_channel *ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	firefly_connection_close(conn);
	event_execute_test(eq, 1);

	size_t len = firefly_event_queue_length(eq);
	expected_error = FIREFLY_ERROR_PROTO_STATE;
	struct labcomm_encoder *ch_enc = firefly_protocol_get_output_stream(ch);
	labcomm_encoder_register_test_test_var(ch_enc);

	CU_ASSERT_TRUE(was_in_error);
	CU_ASSERT_EQUAL(len, firefly_event_queue_length(eq));

	event_execute_test(eq, 3);

	was_in_error = false;
	transport_sent = false;
	expected_error = FIREFLY_ERROR_FIRST;
}

void test_conn_close_send_first()
{
	struct firefly_connection *conn;
	struct firefly_transport_connection test_trsp_conn = {
		.write = transport_write_mock,
		.ack = NULL,
		.open = test_conn_open,
		.close = NULL,
		.context = &conn
	};
	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	struct firefly_channel *ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	struct labcomm_encoder *ch_enc = firefly_protocol_get_output_stream(ch);
	labcomm_encoder_register_test_test_var(ch_enc);

	firefly_connection_close(conn);

	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(transport_sent);
	CU_ASSERT_FALSE(was_in_error);

	event_execute_test(eq, 4);

	transport_sent = false;
}

void test_conn_close_recv_any()
{
	struct firefly_connection *conn;
	struct firefly_transport_connection test_trsp_conn = {
		.write = transport_write_mock,
		.ack = NULL,
		.open = test_conn_open,
		.close = free_plat_conn_test,
		.context = &conn
	};
	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	struct firefly_channel *ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	firefly_connection_close(conn);
	event_execute_test(eq, 1);

	unsigned char mock_data[] = {1,2,3,4,5,6};
	size_t len = firefly_event_queue_length(eq);
	protocol_data_received(conn, mock_data, sizeof(mock_data));
	CU_ASSERT_FALSE(was_in_error);
	CU_ASSERT_EQUAL(len, firefly_event_queue_length(eq));
	event_execute_test(eq, 3);

	transport_sent = false;
	was_in_error = false;
	expected_error = FIREFLY_ERROR_FIRST;
}

static bool chan_accept_called = false;

bool channel_accept_test(struct firefly_channel *ch)
{
	UNUSED_VAR(ch);
	chan_accept_called = true;
	return true;
}

void test_conn_close_recv_chan_req_first()
{
	unsigned char *buf;
	size_t buf_size;
	struct firefly_connection_actions conn_actions = {
		.channel_recv		= channel_accept_test,
		.channel_opened		= NULL,
		.channel_rejected	= NULL,
		.channel_closed		= NULL,
		.channel_restrict	= NULL,
		.channel_restrict_info	= NULL
	};
	struct firefly_connection *conn;
	struct firefly_transport_connection test_trsp_conn = {
		.write = transport_write_mock,
		.ack = NULL,
		.open = test_conn_open,
		.close = free_plat_conn_test,
		.context = &conn
	};
	int res = firefly_connection_open(&conn_actions, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	/* First add close connection event */
	firefly_connection_close(conn);

	/* Without executing the event receive a channel request */
	firefly_protocol_channel_request chan_req;
	chan_req.source_chan_id = 0;
	chan_req.dest_chan_id = CHANNEL_ID_NOT_SET;
	labcomm_encode_firefly_protocol_channel_request(test_enc, &chan_req);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);

	/* Execute all events */
	event_execute_all_test(eq);
	CU_ASSERT_FALSE(was_in_error);
	CU_ASSERT_TRUE(chan_accept_called);

	chan_accept_called = false;
	was_in_error = false;
}
