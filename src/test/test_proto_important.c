
#include <stdbool.h>

#include "CUnit/Basic.h"
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
	}
	*id = TEST_IMPORTANT_ID;
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

	labcomm_encoder_register_test_test_var(
			firefly_protocol_get_output_stream(chan));
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_EQUAL(chan->important_id, TEST_IMPORTANT_ID);

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

	chan->important_id = TEST_IMPORTANT_ID;
	chan->current_seqno = 1;

	firefly_protocol_ack ack_pkt;
	ack_pkt.dest_chan_id = chan->local_id;
	ack_pkt.src_chan_id = chan->remote_id;
	ack_pkt.seqno = 1;
	labcomm_encode_firefly_protocol_ack(test_enc, &ack_pkt);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	CU_ASSERT_TRUE(mock_transport_acked);
	CU_ASSERT_EQUAL(chan->important_id, 0);
	CU_ASSERT_EQUAL(chan->current_seqno, 1);

	mock_transport_written = false;
	mock_transport_acked = false;
	firefly_connection_free(&conn);
}

// TODO test seqnos
