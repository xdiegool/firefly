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

static bool was_in_error = false;
static enum firefly_error expected_error;
// Override.
void firefly_error(enum firefly_error error_id, size_t nbr_va_args, ...)
{
	was_in_error = true;
	CU_ASSERT_EQUAL(expected_error, error_id);
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

	// Register encoders. Signatures will be sent to each connection
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


	firefly_protocol_channel_ack ack;
	ack.dest_chan_id = 14; // Non existing channel.
	ack.source_chan_id = 14;
	CU_ASSERT_EQUAL_FATAL(firefly_event_queue_length(eq), 0);
	labcomm_encode_firefly_protocol_channel_ack(conn_open->transport_encoder,
												&ack);

	expected_error = FIREFLY_ERROR_PROTO_STATE;
	protocol_data_received(conn_recv, conn_open_write.data,
						   conn_open_write.size);
	free_tmp_data(&conn_recv_write);

	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ev);
	firefly_event_execute(ev);

	expected_error = FIREFLY_ERROR_FIRST; // Reset
	CU_ASSERT_TRUE(was_in_error);
	was_in_error = false; // Reset.

	// Clean up
	chan_opened_called = false;
	firefly_connection_free(&conn_open);
	firefly_connection_free(&conn_recv);
	firefly_event_queue_free(&eq);
	free(conn_open_write.data);
}
