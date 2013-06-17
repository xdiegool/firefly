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
#include "protocol/firefly_protocol_private.h"
#include <utils/firefly_errors.h>
#include "utils/cppmacros.h"

static struct firefly_event_queue *eq = NULL;
extern struct labcomm_encoder *test_enc;

int init_suit_proto_conn()
{
	init_labcomm_test_enc_dec();
	eq = firefly_event_queue_new(firefly_event_add, 14, NULL);
	return 0;
}

int clean_suit_proto_conn()
{
	clean_labcomm_test_enc_dec();
	firefly_event_queue_free(&eq);
	return 0;
}

static void execute_remaining_events(struct firefly_event_queue *eq)
{
	struct firefly_event *ev;
	while (firefly_event_queue_length(eq) > 0) {
		ev = firefly_event_pop(eq);
		CU_ASSERT_PTR_NOT_NULL(ev);
		firefly_event_execute(ev);
		firefly_event_return(eq, &ev);
	}
}

static void execute_events(struct firefly_event_queue *eq, size_t nbr_events)
{
	struct firefly_event *ev;
	for (size_t i = 0; i < nbr_events; i++) {
		ev = firefly_event_pop(eq);
		CU_ASSERT_PTR_NOT_NULL(ev);
		firefly_event_execute(ev);
		firefly_event_return(eq, &ev);
	}
}

extern bool was_in_error;
extern enum firefly_error expected_error;

static bool plat_freed = false;

static void free_plat_conn_test(struct firefly_connection *conn)
{
	free(conn->transport_conn_platspec);
	plat_freed = true;
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

void test_conn_close_empty()
{
	char *dummy_data = malloc(5);
	for (int i = 0; i < 5; i++) {
		dummy_data[i] = i;
	}
	struct firefly_connection *conn = firefly_connection_new(
			NULL, NULL, NULL, NULL, eq,
			dummy_data, free_plat_conn_test);

	CU_ASSERT_EQUAL(conn->open, FIREFLY_CONNECTION_OPEN);
	firefly_connection_close(conn);

	execute_events(eq, 1);
	CU_ASSERT_TRUE(plat_freed);

	execute_events(eq, 1);

	plat_freed = false;
	transport_sent = false;
}

void test_conn_close_mult_chans()
{
	char *dummy_data = malloc(5);
	for (int i = 0; i < 5; i++) {
		dummy_data[i] = i;
	}
	struct firefly_connection *conn = firefly_connection_new(
			NULL, transport_write_mock, NULL, NULL,
			eq, dummy_data, free_plat_conn_test);

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
	execute_events(eq, 7);
	CU_ASSERT_FALSE(plat_freed);
	execute_events(eq, 2);
	CU_ASSERT_TRUE(plat_freed);

	transport_sent = false;
}

void test_conn_close_open_chan()
{
	struct firefly_connection *conn = firefly_connection_new(
			NULL, transport_write_mock, NULL, NULL, eq, NULL, NULL);

	struct firefly_channel *ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	firefly_connection_close(conn);
	execute_events(eq, 3);
	struct firefly_event *close_ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(close_ev);

	expected_error = FIREFLY_ERROR_PROTO_STATE;
	firefly_channel_open(conn);
	CU_ASSERT_TRUE(was_in_error);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NULL(ev);

	firefly_event_execute(close_ev);
	firefly_event_return(eq, &close_ev);
	execute_events(eq, 1);

	was_in_error = false;
	transport_sent = false;
	expected_error = FIREFLY_ERROR_FIRST;
}

void test_conn_close_send_data()
{
	struct firefly_connection *conn = firefly_connection_new(
			NULL, transport_write_mock, NULL, NULL, eq, NULL, NULL);

	struct firefly_channel *ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	firefly_connection_close(conn);
	struct firefly_event **close_ev;
	close_ev = calloc(4, sizeof(struct firefly_event*));
	close_ev[0] = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(close_ev[0]);
	firefly_event_execute(close_ev[0]);
	firefly_event_return(eq, &close_ev[0]);

	close_ev[1] = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(close_ev[1]);
	close_ev[2] = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(close_ev[2]);
	close_ev[3] = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(close_ev[3]);

	expected_error = FIREFLY_ERROR_PROTO_STATE;
	struct labcomm_encoder *ch_enc = firefly_protocol_get_output_stream(ch);
	labcomm_encoder_register_test_test_var(ch_enc);

	CU_ASSERT_TRUE(was_in_error);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NULL(ev);

	firefly_event_execute(close_ev[1]);
	firefly_event_return(eq, &close_ev[1]);
	firefly_event_execute(close_ev[2]);
	firefly_event_return(eq, &close_ev[2]);
	firefly_event_execute(close_ev[3]);
	firefly_event_return(eq, &close_ev[3]);
	execute_events(eq, 1);
	free(close_ev);

	was_in_error = false;
	transport_sent = false;
	expected_error = FIREFLY_ERROR_FIRST;
}

void test_conn_close_send_first()
{
	struct firefly_connection *conn = firefly_connection_new(
			NULL, transport_write_mock, NULL, NULL, eq, NULL, NULL);

	struct firefly_channel *ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);
	struct labcomm_encoder *ch_enc = firefly_protocol_get_output_stream(ch);
	labcomm_encoder_register_test_test_var(ch_enc);
	firefly_connection_close(conn);

	struct firefly_event *send_ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(send_ev);
	firefly_event_execute(send_ev);
	firefly_event_return(eq, &send_ev);

	CU_ASSERT_TRUE(transport_sent);
	CU_ASSERT_FALSE(was_in_error);

	execute_events(eq, 5);

	transport_sent = false;
}

void test_conn_close_recv_any()
{
	struct firefly_connection *conn = firefly_connection_new(
			NULL, transport_write_mock, NULL, NULL, eq, NULL, NULL);

	struct firefly_channel *ch = firefly_channel_new(conn);
	ch->remote_id = 0;
	add_channel_to_connection(ch, conn);

	firefly_connection_close(conn);
	struct firefly_event *close_ev;
	close_ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(close_ev);
	firefly_event_execute(close_ev);
	firefly_event_return(eq, &close_ev);

	unsigned char mock_data[] = {1,2,3,4,5,6};
	protocol_data_received(conn, mock_data, sizeof(mock_data));
	CU_ASSERT_FALSE(was_in_error);
	execute_events(eq, 4);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NULL(ev);

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
	struct firefly_connection *conn = firefly_connection_new(
			&conn_actions,
			transport_write_mock, NULL, NULL, eq,
			NULL, NULL);

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
	execute_remaining_events(eq);
	CU_ASSERT_FALSE(was_in_error);
	CU_ASSERT_TRUE(chan_accept_called);

	chan_accept_called = false;
	was_in_error = false;
}
