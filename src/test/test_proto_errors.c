#include "test/test_proto_errors.h"

#include <stdbool.h>

#include <CUnit/Basic.h>
#include <labcomm.h>

#include <gen/firefly_protocol.h>
#include <gen/test.h>
#include <protocol/firefly_protocol.h>
#include <utils/firefly_errors.h>
#include <utils/firefly_event_queue.h>

#include "protocol/firefly_protocol_private.h"
#include "proto_helper.h"
#include "utils/cppmacros.h"

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

bool was_in_error = false;
enum firefly_error expected_error;

// Override.
void firefly_error(enum firefly_error error_id, size_t nbr_va_args, ...)
{
	UNUSED_VAR(nbr_va_args);
	was_in_error = true;
	/*printf("Error: %d\n", error_id);*/
	/*if (nbr_va_args >= 1) {*/
		/*va_list arg_pointer;*/
		/*va_start(arg_pointer, nbr_va_args);*/
		/*char *print_format = va_arg(arg_pointer, char *);*/
		/*vfprintf(stderr, print_format, arg_pointer);*/
	/*}*/
	CU_ASSERT_EQUAL(expected_error, error_id);
}

// Override.
void labcomm_error_to_ff_error(enum labcomm_error error_id, size_t nbr_va_args,
									...)
{
	UNUSED_VAR(error_id);
	UNUSED_VAR(nbr_va_args);
	const char *lc_err_msg = labcomm_error_get_str(error_id);
	firefly_error(FIREFLY_ERROR_LABCOMM, 1, lc_err_msg);
}


static void mk_lc_and_reg_sigs_free(struct firefly_connection *conn_open,
					struct firefly_connection *conn_recv,
					struct firefly_event_queue *eq)
{
	// Clean up
	chan_opened_called = false;
	firefly_connection_free(&conn_open);
	firefly_connection_free(&conn_recv);
	firefly_event_queue_free(&eq);
}


static void mk_lc_and_reg_sigs(struct firefly_connection **conn_open,
				   struct firefly_connection **conn_recv,
				   struct firefly_event_queue **eq)
{
	*eq = firefly_event_queue_new(firefly_event_add, NULL);
	if (*eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	// Init connection to open channel
	*conn_open = setup_test_conn_new(chan_opened_mock, NULL,
						chan_open_recv_accept_open,
						*eq);
	(*conn_open)->transport_write = chan_open_recv_write_open;

	// Init connection to receive channel
	*conn_recv = setup_test_conn_new(chan_opened_mock, NULL,
					chan_open_recv_accept_recv, *eq);
	(*conn_recv)->transport_write = chan_open_recv_write_recv;
}

void test_unexpected_ack()
{
	struct firefly_connection *conn_open;
	struct firefly_connection *conn_recv;
	struct firefly_event_queue *eq;

	mk_lc_and_reg_sigs(&conn_open, &conn_recv, &eq);

	firefly_protocol_channel_ack ack;
	ack.dest_chan_id = 14; // Non existing channel.
	ack.source_chan_id = 14;
	CU_ASSERT_EQUAL_FATAL(firefly_event_queue_length(eq), 0);
	labcomm_encode_firefly_protocol_channel_ack(
					conn_open->transport_encoder, &ack);

	expected_error = FIREFLY_ERROR_PROTO_STATE;
	protocol_data_received(conn_recv, conn_open_write.data,
						   conn_open_write.size);

	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ev);
	firefly_event_execute(ev);

	expected_error = FIREFLY_ERROR_FIRST; // Reset
	CU_ASSERT_TRUE(was_in_error);
	was_in_error = false; // Reset.
	free(conn_open_write.data);

	mk_lc_and_reg_sigs_free(conn_open, conn_recv, eq);
}

void test_unexpected_response()
{
	struct firefly_connection *conn_open;
	struct firefly_connection *conn_recv;
	struct firefly_event_queue *eq;

	mk_lc_and_reg_sigs(&conn_open, &conn_recv, &eq);

	firefly_protocol_channel_response resp;
	resp.dest_chan_id = 14;
	resp.source_chan_id = 14;
	resp.ack = true;
	CU_ASSERT_EQUAL_FATAL(firefly_event_queue_length(eq), 0);
	labcomm_encode_firefly_protocol_channel_response(
					conn_recv->transport_encoder, &resp);

	expected_error = FIREFLY_ERROR_PROTO_STATE;
	protocol_data_received(conn_open, conn_recv_write.data,
						   conn_recv_write.size);

	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ev);
	firefly_event_execute(ev);

	expected_error = FIREFLY_ERROR_FIRST; // Reset
	CU_ASSERT_TRUE(was_in_error);
	was_in_error = false; // Reset.
	free(conn_open_write.data);
	free(conn_recv_write.data);

	mk_lc_and_reg_sigs_free(conn_open, conn_recv, eq);
}

void test_unexpected_data_sample()
{
	struct firefly_connection *conn_open;
	struct firefly_connection *conn_recv;
	struct firefly_event_queue *eq;
	const size_t app_data_size = 12;

	mk_lc_and_reg_sigs(&conn_open, &conn_recv, &eq);

	unsigned char *app_data = malloc(app_data_size);

	firefly_protocol_data_sample sample;
	sample.dest_chan_id = 14;
	sample.src_chan_id = 15;
	sample.app_enc_data.n_0 = app_data_size;
	sample.app_enc_data.a = app_data;
	CU_ASSERT_EQUAL_FATAL(firefly_event_queue_length(eq), 0);

	labcomm_encode_firefly_protocol_data_sample(
			conn_open->transport_encoder, &sample);

	expected_error = FIREFLY_ERROR_PROTO_STATE;
	protocol_data_received(conn_recv, conn_open_write.data,
						   conn_open_write.size);

	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ev);
	firefly_event_execute(ev);

	CU_ASSERT_TRUE(was_in_error);
	expected_error = FIREFLY_ERROR_FIRST; // Reset
	was_in_error = false; // Reset.

	/*
	 * There should now be a chan_close as response to the erroneous app_data.
	 * Any error is detected immediately, without the use of event.
	 * TODO: This is inconsistent. The channel is searched for before creating
	 * the event. For app data this is done when executing the event.
	 */
	CU_ASSERT_PTR_NOT_NULL(conn_open);
	CU_ASSERT_PTR_NULL(conn_open->chan_list);
	conn_open->chan_list = malloc(sizeof(struct channel_list_node));
	conn_open->chan_list->next = NULL;
	conn_open->chan_list->chan = firefly_channel_new(conn_open);
	conn_open->chan_list->chan->local_id = 15;
	conn_open->chan_list->chan->remote_id = 14;

	firefly_protocol_channel_close close_msg;

	protocol_data_received(conn_open, conn_recv_write.data,
						   conn_recv_write.size);
	CU_ASSERT_FALSE(was_in_error);
	expected_error = FIREFLY_ERROR_FIRST; // Reset
	was_in_error = false; // Reset.

	free(conn_open_write.data);

	mk_lc_and_reg_sigs_free(conn_open, conn_recv, eq);
	free(app_data);
}

void test_unexpected_channel_close()
{
	struct firefly_connection *conn_open;
	struct firefly_connection *conn_recv;
	struct firefly_event_queue *eq;

	mk_lc_and_reg_sigs(&conn_open, &conn_recv, &eq);

	firefly_protocol_channel_close close;
	close.dest_chan_id = 14; // Non existing channel.
	close.source_chan_id = 14;
	CU_ASSERT_EQUAL_FATAL(firefly_event_queue_length(eq), 0);
	labcomm_encode_firefly_protocol_channel_close(
					conn_open->transport_encoder, &close);

	expected_error = FIREFLY_ERROR_PROTO_STATE;
	protocol_data_received(conn_recv, conn_open_write.data,
						   conn_open_write.size);

	expected_error = FIREFLY_ERROR_FIRST; // Reset
	CU_ASSERT_TRUE(was_in_error);
	was_in_error = false; // Reset.
	free(conn_open_write.data);

	mk_lc_and_reg_sigs_free(conn_open, conn_recv, eq);
}
