
#include <stdbool.h>

#include "CUnit/Basic.h"
#include <limits.h>
#include <labcomm.h>
#include <test/labcomm_mem_writer.h>
#include <test/labcomm_mem_reader.h>

#include <utils/firefly_event_queue.h>
#include <utils/cppmacros.h>
#include <protocol/firefly_protocol.h>
#include <gen/test.h>

#include <protocol/firefly_protocol_private.h>
#include "test/event_helper.h"
#include "test/proto_helper.h"

#define TEST_IMPORTANT_ID 1


extern struct labcomm_decoder *test_dec;
extern labcomm_mem_reader_context_t *test_dec_ctx;

extern struct labcomm_encoder *test_enc;
extern labcomm_mem_writer_context_t *test_enc_ctx;

extern firefly_protocol_data_sample data_sample;
extern firefly_protocol_ack ack;
extern bool received_ack;

struct firefly_event_queue *eq;

struct test_conn_platspec {
	bool important;
};

int init_suit_proto_important()
{
	init_labcomm_test_enc_dec();
	eq = firefly_event_queue_new(firefly_event_add, NULL);
	if (eq == NULL) {
		return 1;
	}
	return 0;
}

int clean_suit_proto_important()
{
	clean_labcomm_test_enc_dec();
	firefly_event_queue_free(&eq);
	return 0;
}

bool mock_transport_written = false;
void mock_transport_write_important(unsigned char *data, size_t size,
		struct firefly_connection *conn, bool important, unsigned char *id)
{
	UNUSED_VAR(data);
	UNUSED_VAR(size);
	mock_transport_written = true;
	struct test_conn_platspec *ps =
		(struct test_conn_platspec *) conn->transport_conn_platspec;
	CU_ASSERT_EQUAL(ps->important, important);
	if (important) {
		CU_ASSERT_PTR_NOT_NULL(id);
		*id = TEST_IMPORTANT_ID;
	}
	transport_write_test_decoder(data, size, conn, important, id);
}

bool mock_transport_acked = false;
void mock_transport_ack(unsigned char id, struct firefly_connection *conn)
{
	UNUSED_VAR(conn);
	CU_ASSERT_EQUAL(id, TEST_IMPORTANT_ID);
	mock_transport_acked = true;
}

void test_important_signature()
{
	struct test_conn_platspec ps;
	ps.important = true;
	struct firefly_connection *conn = firefly_connection_new(NULL, NULL, NULL,
			mock_transport_write_important, NULL, eq, &ps, NULL);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	CU_ASSERT_EQUAL(chan->current_seqno, 0);
	labcomm_encoder_register_test_test_var(
			firefly_protocol_get_output_stream(chan));
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(data_sample.important);
	CU_ASSERT_PTR_NOT_NULL_FATAL(chan->important_packet);
	CU_ASSERT_EQUAL(chan->important_packet->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->important_packet->seqno, 1);
	CU_ASSERT_EQUAL(chan->current_seqno, 1);

	mock_transport_written = false;
	firefly_connection_free(&conn);
}

void test_important_recv_ack()
{
	struct test_conn_platspec ps;
	ps.important = true;
	struct firefly_connection *conn = firefly_connection_new(NULL, NULL, NULL,
			mock_transport_write_important, mock_transport_ack, eq, &ps, NULL);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	chan->important_packet = malloc(sizeof(struct firefly_channel_important_packet));
	chan->important_packet->seqno = 1;
	chan->important_packet->important_id = TEST_IMPORTANT_ID;
	chan->current_seqno = 1;

	firefly_protocol_ack ack_pkt;
	ack_pkt.dest_chan_id = chan->local_id;
	ack_pkt.src_chan_id = chan->remote_id;
	ack_pkt.seqno = 1;
	labcomm_encode_firefly_protocol_ack(test_enc, &ack_pkt);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	CU_ASSERT_TRUE(mock_transport_acked);
	CU_ASSERT_PTR_NULL(chan->important_packet);
	CU_ASSERT_EQUAL(chan->current_seqno, 1);

	mock_transport_written = false;
	mock_transport_acked = false;
	firefly_connection_free(&conn);
}

void test_important_signatures_mult()
{
	struct test_conn_platspec ps;
	ps.important = true;
	struct firefly_connection *conn = firefly_connection_new(NULL, NULL, NULL,
			mock_transport_write_important, NULL, eq, &ps, NULL);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	CU_ASSERT_EQUAL(chan->current_seqno, 0);
	labcomm_encoder_register_test_test_var(
			firefly_protocol_get_output_stream(chan));
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(data_sample.important);
	CU_ASSERT_PTR_NOT_NULL_FATAL(chan->important_packet);
	CU_ASSERT_EQUAL(chan->important_packet->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->important_packet->seqno, 1);
	CU_ASSERT_EQUAL(chan->current_seqno, 1);

	// simulate ack received
	free(chan->important_packet);
	chan->important_packet = NULL;

	labcomm_encoder_register_test_test_var_2(
			firefly_protocol_get_output_stream(chan));

	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(data_sample.important);
	CU_ASSERT_PTR_NOT_NULL_FATAL(chan->important_packet);
	CU_ASSERT_EQUAL(chan->important_packet->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->important_packet->seqno, 2);
	CU_ASSERT_EQUAL(chan->current_seqno, 2);

	mock_transport_written = false;
	firefly_connection_free(&conn);
}

void test_important_seqno_overflow()
{
	struct test_conn_platspec ps;
	ps.important = true;
	struct firefly_connection *conn = firefly_connection_new(NULL, NULL, NULL,
			mock_transport_write_important, NULL, eq, &ps, NULL);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	chan->current_seqno = INT_MAX;
	labcomm_encoder_register_test_test_var(
			firefly_protocol_get_output_stream(chan));
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(data_sample.important);
	CU_ASSERT_PTR_NOT_NULL_FATAL(chan->important_packet);
	CU_ASSERT_EQUAL(chan->important_packet->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->important_packet->seqno, 1);
	CU_ASSERT_EQUAL(chan->current_seqno, 1);

	mock_transport_written = false;
	firefly_connection_free(&conn);
}

void test_important_send_ack()
{
	struct test_conn_platspec ps;
	ps.important = true;
	struct firefly_connection *conn = firefly_connection_new(NULL, NULL, NULL,
			transport_write_test_decoder, NULL, eq, &ps, NULL);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	firefly_protocol_data_sample sample_pkt;
	sample_pkt.dest_chan_id = chan->local_id;
	sample_pkt.src_chan_id = chan->remote_id;
	sample_pkt.seqno = 1;
	sample_pkt.important = true;
	sample_pkt.app_enc_data.a = NULL;
	sample_pkt.app_enc_data.n_0 = 0;
	labcomm_encode_firefly_protocol_data_sample(test_enc, &sample_pkt);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(received_ack);
	CU_ASSERT_EQUAL(ack.seqno, 1);

	received_ack = false;
	firefly_connection_free(&conn);
}

void test_not_important_not_send_ack()
{
	struct test_conn_platspec ps;
	ps.important = true;
	struct firefly_connection *conn = firefly_connection_new(NULL, NULL, NULL,
			transport_write_test_decoder, NULL, eq, &ps, NULL);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	firefly_protocol_data_sample sample_pkt;
	sample_pkt.dest_chan_id = chan->local_id;
	sample_pkt.src_chan_id = chan->remote_id;
	sample_pkt.seqno = 0;
	sample_pkt.important = false;
	sample_pkt.app_enc_data.a = NULL;
	sample_pkt.app_enc_data.n_0 = 0;
	labcomm_encode_firefly_protocol_data_sample(test_enc, &sample_pkt);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	event_execute_test(eq, 1);

	CU_ASSERT_FALSE(received_ack);

	received_ack = false;
	firefly_connection_free(&conn);
}

void test_errorneous_ack()
{
	struct test_conn_platspec ps;
	ps.important = false;
	struct firefly_connection *conn = firefly_connection_new(NULL, NULL, NULL,
			transport_write_test_decoder, NULL, eq, &ps, NULL);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	chan->important_packet = malloc(sizeof(struct firefly_channel_important_packet));
	chan->important_packet->seqno = 5;
	chan->important_packet->important_id = TEST_IMPORTANT_ID;
	chan->current_seqno = 5;

	firefly_protocol_ack ack_pkt;
	ack_pkt.dest_chan_id = chan->local_id;
	ack_pkt.src_chan_id = chan->remote_id;
	ack_pkt.seqno = 4;
	labcomm_encode_firefly_protocol_ack(test_enc, &ack_pkt);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	CU_ASSERT_FALSE(mock_transport_acked);
	CU_ASSERT_PTR_NOT_NULL_FATAL(chan->important_packet);
	CU_ASSERT_EQUAL(chan->important_packet->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->important_packet->seqno, 5);
	CU_ASSERT_EQUAL(chan->current_seqno, 5);

	ack_pkt.seqno = 6;
	labcomm_encode_firefly_protocol_ack(test_enc, &ack_pkt);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	CU_ASSERT_FALSE(mock_transport_acked);
	CU_ASSERT_PTR_NOT_NULL_FATAL(chan->important_packet);
	CU_ASSERT_EQUAL(chan->important_packet->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->important_packet->seqno, 5);
	CU_ASSERT_EQUAL(chan->current_seqno, 5);

	mock_transport_written = false;
	mock_transport_acked = false;
	firefly_connection_free(&conn);
}

void test_multiple_simultaneous_important()
{
	CU_FAIL("NOT IMPLEMENTED");
}
// TODO test multiple important queue
