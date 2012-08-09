#include "test/test_proto_errors.h"

#include "CUnit/Basic.h"

#include <labcomm.h>

#include <stdbool.h>

#include <gen/firefly_protocol.h>
#include <gen/test.h>
#include <firefly_errors.h>
#include "protocol/firefly_protocol_private.h"
#include <protocol/firefly_protocol.h>
#include "eventqueue/event_queue.h"
#include "proto_helper.h"

extern struct tmp_data conn_open_write;
extern struct tmp_data conn_recv_write;
extern bool chan_opened_called;
extern bool conn_recv_accept_called;

int init_suite_proto_errors()
{
	return 0;
}

int clean_suite_proto_errors()
{
	return 0;
}

void test_unexpected_ack()
{
	struct firefly_connection *conn_open;
	struct firefly_connection *conn_recv;

	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	// Init connection to open channel
	conn_open = setup_test_conn_new(chan_opened_mock, NULL,
									   chan_open_recv_accept_open, eq);
	conn_open->transport_write = chan_open_recv_write_open;

	// Init connection to receive channel
	conn_recv = setup_test_conn_new(chan_opened_mock, NULL,
									chan_open_recv_accept_recv, eq);
	conn_recv->transport_write = chan_open_recv_write_recv;

	// register decoders
	labcomm_decoder_register_firefly_protocol_channel_response(
			conn_open->transport_decoder, handle_channel_response, conn_open);
	labcomm_decoder_register_firefly_protocol_channel_request(
			conn_recv->transport_decoder, handle_channel_request, conn_recv);
	labcomm_decoder_register_firefly_protocol_channel_ack(
			conn_recv->transport_decoder, handle_channel_ack, conn_recv);

	// register encoders Signatures will be sent to each connection
	labcomm_encoder_register_firefly_protocol_channel_ack(
			conn_open->transport_encoder);
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	free_tmp_data(&conn_open_write);

	labcomm_encoder_register_firefly_protocol_channel_request(
			conn_open->transport_encoder);
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	free_tmp_data(&conn_open_write);

	labcomm_encoder_register_firefly_protocol_channel_response(
			conn_recv->transport_encoder);
	protocol_data_received(conn_open, conn_recv_write.data,
			conn_recv_write.size);
	free_tmp_data(&conn_recv_write);

	// Init open channel from conn_open
	firefly_channel_open(conn_open, NULL);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	free_tmp_data(&conn_open_write);
	CU_ASSERT_TRUE(conn_recv_accept_called);
	protocol_data_received(conn_open, conn_recv_write.data,
						   conn_recv_write.size);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);
	CU_ASSERT_TRUE(chan_opened_called);
	chan_opened_called = false;

	free_tmp_data(&conn_recv_write);
	protocol_data_received(conn_recv, conn_open_write.data,
			conn_open_write.size);
	free_tmp_data(&conn_open_write);
	ev = firefly_event_pop(eq);
	firefly_event_execute(ev);
	CU_ASSERT_TRUE(chan_opened_called);
	chan_opened_called = false;

	CU_ASSERT_PTR_NOT_NULL(conn_open->chan_list->chan);
	CU_ASSERT_PTR_NOT_NULL(conn_recv->chan_list->chan);
	CU_ASSERT_EQUAL(conn_recv->chan_list->chan->local_id, conn_open->chan_list->chan->remote_id);
	CU_ASSERT_EQUAL(conn_recv->chan_list->chan->remote_id, conn_open->chan_list->chan->local_id);

	// Clean up
	chan_opened_called = false;
	firefly_connection_free(&conn_open);
	firefly_connection_free(&conn_recv);
	firefly_event_queue_free(&eq);
}
