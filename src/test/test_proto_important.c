
#include <stdbool.h>

#include "CUnit/Basic.h"
#include <limits.h>
#include <labcomm.h>
#include <labcomm_ioctl.h>
#include <labcomm_static_buffer_reader.h>
#include <labcomm_static_buffer_writer.h>

#include <utils/firefly_event_queue.h>
#include <utils/cppmacros.h>
#include <protocol/firefly_protocol.h>
#include <gen/test.h>

#include <protocol/firefly_protocol_private.h>
#include "test/event_helper.h"
#include "test/proto_helper.h"

#define TEST_IMPORTANT_ID 1


extern struct labcomm_decoder *test_dec;

extern struct labcomm_encoder *test_enc;

extern firefly_protocol_data_sample data_sample;
extern firefly_protocol_ack ack;
extern firefly_protocol_channel_request channel_request;
extern firefly_protocol_channel_response channel_response;
extern firefly_protocol_channel_ack channel_ack;
extern bool received_ack;

struct firefly_event_queue *eq;

struct test_conn_platspec {
	bool important;
	struct firefly_connection **conn;
};

int init_suit_proto_important()
{
	init_labcomm_test_enc_dec();
	eq = firefly_event_queue_new(firefly_event_add, 10, NULL);
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
		(struct test_conn_platspec *) conn->transport->context;
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

static int test_conn_open(struct firefly_connection *conn)
{
	struct test_conn_platspec *ps = conn->transport->context;
	struct firefly_connection **res = ps->conn;
	*res = conn;
	return 0;
}

void test_important_signature()
{
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = true, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = mock_transport_write_important,
		.ack = NULL,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	CU_ASSERT_EQUAL(chan->current_seqno, 0);
	labcomm_encoder_register_test_test_var(
			firefly_protocol_get_output_stream(chan));
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(data_sample.important);
	CU_ASSERT_EQUAL(chan->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->current_seqno, 1);

	mock_transport_written = false;
	firefly_connection_free(&conn);
}

void test_important_recv_ack()
{
	unsigned char *buf;
	size_t buf_size;
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = true, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = mock_transport_write_important,
		.ack = mock_transport_ack,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	chan->important_id = TEST_IMPORTANT_ID;
	chan->current_seqno = 1;

	firefly_protocol_ack ack_pkt;
	ack_pkt.dest_chan_id = chan->local_id;
	ack_pkt.src_chan_id = chan->remote_id;
	ack_pkt.seqno = 1;
	labcomm_encode_firefly_protocol_ack(test_enc, &ack_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);
	CU_ASSERT_TRUE(mock_transport_acked);
	CU_ASSERT_EQUAL(chan->important_id, 0);
	CU_ASSERT_EQUAL(chan->current_seqno, 1);

	mock_transport_written = false;
	mock_transport_acked = false;
	firefly_connection_free(&conn);
}

void test_important_signatures_mult()
{
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = true, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = mock_transport_write_important,
		.ack = NULL,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	CU_ASSERT_EQUAL(chan->current_seqno, 0);
	labcomm_encoder_register_test_test_var(
			firefly_protocol_get_output_stream(chan));
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(data_sample.important);
	CU_ASSERT_EQUAL(chan->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->current_seqno, 1);

	// simulate ack received
	chan->important_id = 0;

	labcomm_encoder_register_test_test_var_2(
			firefly_protocol_get_output_stream(chan));

	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(data_sample.important);
	CU_ASSERT_EQUAL(chan->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->current_seqno, 2);

	mock_transport_written = false;
	firefly_connection_free(&conn);
}

void test_important_seqno_overflow()
{
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = true, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = mock_transport_write_important,
		.ack = NULL,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	chan->current_seqno = INT_MAX;
	labcomm_encoder_register_test_test_var(
			firefly_protocol_get_output_stream(chan));
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(data_sample.important);
	CU_ASSERT_EQUAL(chan->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->current_seqno, 1);

	mock_transport_written = false;
	firefly_connection_free(&conn);
}

void test_important_send_ack()
{
	unsigned char *buf;
	size_t buf_size;
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = true, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = transport_write_test_decoder,
		.ack = NULL,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
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
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(received_ack);
	CU_ASSERT_EQUAL(ack.seqno, 1);
	CU_ASSERT_EQUAL(chan->remote_seqno, 1);

	received_ack = false;
	firefly_connection_free(&conn);
}

void test_not_important_not_send_ack()
{
	unsigned char *buf;
	size_t buf_size;
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = true, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = transport_write_test_decoder,
		.ack = NULL,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
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
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);
	event_execute_test(eq, 1);

	CU_ASSERT_FALSE(received_ack);

	received_ack = false;
	firefly_connection_free(&conn);
}

void test_important_mult_simultaneously()
{
	unsigned char *buf;
	size_t buf_size;
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = true, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = mock_transport_write_important,
		.ack = mock_transport_ack,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	firefly_protocol_ack ack_pkt;
	ack_pkt.dest_chan_id = chan->local_id;
	ack_pkt.src_chan_id = chan->remote_id;

	CU_ASSERT_EQUAL(chan->current_seqno, 0);
	labcomm_encoder_register_test_test_var(
			firefly_protocol_get_output_stream(chan));

	labcomm_encoder_register_test_test_var_2(
			firefly_protocol_get_output_stream(chan));

	labcomm_encoder_register_test_test_var_3(
			firefly_protocol_get_output_stream(chan));

	event_execute_all_test(eq);
	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(data_sample.important);
	CU_ASSERT_EQUAL(chan->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->current_seqno, 1);
	mock_transport_written = false;

	ack_pkt.seqno = 1;
	labcomm_encode_firefly_protocol_ack(test_enc, &ack_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);

	event_execute_all_test(eq);

	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(data_sample.important);
	CU_ASSERT_EQUAL(chan->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->current_seqno, 2);
	mock_transport_written = false;

	ack_pkt.seqno = 2;
	labcomm_encode_firefly_protocol_ack(test_enc, &ack_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);

	event_execute_all_test(eq);

	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(data_sample.important);
	CU_ASSERT_EQUAL(chan->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->current_seqno, 3);
	mock_transport_written = false;

	ack_pkt.seqno = 3;
	labcomm_encode_firefly_protocol_ack(test_enc, &ack_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);

	event_execute_all_test(eq);

	CU_ASSERT_FALSE(mock_transport_written);
	CU_ASSERT_EQUAL(chan->important_id, 0);
	CU_ASSERT_EQUAL(chan->current_seqno, 3);

	mock_transport_written = false;
	firefly_connection_free(&conn);

}

void test_errorneous_ack()
{
	unsigned char *buf;
	size_t buf_size;
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = false, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = mock_transport_write_important,
		.ack = NULL,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	chan->current_seqno = 5;
	chan->important_id = TEST_IMPORTANT_ID;

	firefly_protocol_ack ack_pkt;
	ack_pkt.dest_chan_id = chan->local_id;
	ack_pkt.src_chan_id = chan->remote_id;
	ack_pkt.seqno = 4;
	labcomm_encode_firefly_protocol_ack(test_enc, &ack_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);
	CU_ASSERT_FALSE(mock_transport_acked);
	CU_ASSERT_EQUAL(chan->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->current_seqno, 5);

	ack_pkt.seqno = 6;
	labcomm_encode_firefly_protocol_ack(test_enc, &ack_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);
	CU_ASSERT_FALSE(mock_transport_acked);
	CU_ASSERT_EQUAL(chan->important_id, TEST_IMPORTANT_ID);
	CU_ASSERT_EQUAL(chan->current_seqno, 5);

	mock_transport_written = false;
	mock_transport_acked = false;
	firefly_connection_free(&conn);
}

void handle_test_var_error(test_test_var *d, void *context)
{
	UNUSED_VAR(d);
	UNUSED_VAR(context);
	CU_FAIL("Received test var data but should not have");
}

void test_important_recv_duplicate()
{
	unsigned char *buf;
	size_t buf_size;
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = false, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = transport_write_test_decoder,
		.ack = NULL,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	CU_ASSERT_EQUAL(chan->remote_seqno, 0);

	labcomm_decoder_register_test_test_var(
			firefly_protocol_get_input_stream(chan), handle_test_var_error, NULL);
	labcomm_encoder_register_test_test_var(test_enc);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	firefly_protocol_data_sample sample_pkt;
	sample_pkt.dest_chan_id = chan->local_id;
	sample_pkt.src_chan_id = chan->remote_id;
	sample_pkt.seqno = 1;
	sample_pkt.important = true;
	sample_pkt.app_enc_data.a = malloc(buf_size);
	memcpy(sample_pkt.app_enc_data.a, buf, buf_size);
	sample_pkt.app_enc_data.n_0 = buf_size;
	labcomm_encode_firefly_protocol_data_sample(test_enc, &sample_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(received_ack);
	CU_ASSERT_EQUAL(ack.seqno, 1);
	CU_ASSERT_EQUAL(chan->remote_seqno, 1);

	free(sample_pkt.app_enc_data.a);

	test_test_var app_data = 1;
	labcomm_encode_test_test_var(test_enc, &app_data);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	sample_pkt.dest_chan_id = chan->local_id;
	sample_pkt.src_chan_id = chan->remote_id;
	sample_pkt.seqno = 1;
	sample_pkt.important = true;
	sample_pkt.app_enc_data.a = malloc(buf_size);
	memcpy(sample_pkt.app_enc_data.a, buf, buf_size);
	sample_pkt.app_enc_data.n_0 = buf_size;
	labcomm_encode_firefly_protocol_data_sample(test_enc, &sample_pkt);

	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(received_ack);
	CU_ASSERT_EQUAL(ack.seqno, 1);
	CU_ASSERT_EQUAL(chan->remote_seqno, 1);

	free(sample_pkt.app_enc_data.a);

	mock_transport_written = false;
	mock_transport_acked = false;
	firefly_connection_free(&conn);
}

bool handshake_chan_open_called = false;
void important_handshake_chan_open(struct firefly_channel *chan)
{
	CU_ASSERT_EQUAL(chan->important_id, 0);
	handshake_chan_open_called = true;
}


bool handshake_chan_recv_called = false;
bool important_handshake_chan_acc(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	handshake_chan_recv_called = true;
	return true;
}

void test_important_handshake_recv()
{
	struct firefly_connection_actions conn_actions = {
		.channel_recv		= important_handshake_chan_acc,
		.channel_opened		= important_handshake_chan_open,
		.channel_closed		= NULL,
		.channel_restrict	= NULL,
		.channel_restrict_info	= NULL
	};
	unsigned char *buf;
	size_t buf_size;
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = true, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = mock_transport_write_important,
		.ack = mock_transport_ack,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(&conn_actions, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	firefly_protocol_channel_request req_pkt;
	req_pkt.source_chan_id = 1;
	req_pkt.dest_chan_id = CHANNEL_ID_NOT_SET;
	labcomm_encode_firefly_protocol_channel_request(test_enc, &req_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(mock_transport_written);
	mock_transport_written = false;

	firefly_protocol_channel_ack ack_pkt;
	ack_pkt.source_chan_id = 1;
	ack_pkt.dest_chan_id = channel_response.source_chan_id;
	labcomm_encode_firefly_protocol_channel_ack(test_enc, &ack_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);
	event_execute_test(eq, 1);
	CU_ASSERT_FALSE(mock_transport_written);
	CU_ASSERT_TRUE(mock_transport_acked);
	CU_ASSERT_TRUE(handshake_chan_open_called);


	handshake_chan_open_called = false;
	mock_transport_acked = false;
	mock_transport_written = false;
	handshake_chan_recv_called = false;
	firefly_connection_free(&conn);
}

void test_important_handshake_recv_errors()
{
	struct firefly_connection_actions conn_actions = {
		.channel_recv		= important_handshake_chan_acc,
		.channel_opened		= important_handshake_chan_open,
		.channel_closed		= NULL,
		.channel_restrict	= NULL,
		.channel_restrict_info	= NULL
	};
	unsigned char *buf;
	size_t buf_size;
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = true, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = mock_transport_write_important,
		.ack = mock_transport_ack,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(&conn_actions, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	firefly_protocol_channel_request req_pkt;
	req_pkt.source_chan_id = 1;
	req_pkt.dest_chan_id = CHANNEL_ID_NOT_SET;
	labcomm_encode_firefly_protocol_channel_request(test_enc, &req_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);

	protocol_data_received(conn, buf, buf_size);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(handshake_chan_recv_called);
	mock_transport_written = false;
	handshake_chan_recv_called = false;

	protocol_data_received(conn, buf, buf_size);
	event_execute_test(eq, 1);

	CU_ASSERT_FALSE(mock_transport_written);
	CU_ASSERT_FALSE(handshake_chan_recv_called);
	mock_transport_written = false;
	handshake_chan_recv_called = false;

	firefly_protocol_channel_ack ack_pkt;
	ack_pkt.source_chan_id = 1;
	ack_pkt.dest_chan_id = channel_response.source_chan_id;
	labcomm_encode_firefly_protocol_channel_ack(test_enc, &ack_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);
	event_execute_test(eq, 1);
	CU_ASSERT_FALSE(mock_transport_written);
	CU_ASSERT_TRUE(mock_transport_acked);
	CU_ASSERT_TRUE(handshake_chan_open_called);
	handshake_chan_open_called = false;
	mock_transport_acked = false;
	mock_transport_written = false;

	protocol_data_received(conn, buf, buf_size);
	event_execute_test(eq, 1);
	CU_ASSERT_FALSE(mock_transport_written);
	CU_ASSERT_FALSE(mock_transport_acked);
	CU_ASSERT_FALSE(handshake_chan_open_called);

	handshake_chan_open_called = false;
	mock_transport_acked = false;
	mock_transport_written = false;
	firefly_connection_free(&conn);
}

void test_important_handshake_open()
{
	struct firefly_connection_actions conn_actions = {
		.channel_recv		= important_handshake_chan_acc,
		.channel_opened		= important_handshake_chan_open,
		.channel_closed		= NULL,
		.channel_restrict	= NULL,
		.channel_restrict_info	= NULL
	};
	unsigned char *buf;
	size_t buf_size;
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = true, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = mock_transport_write_important,
		.ack = mock_transport_ack,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(&conn_actions, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	firefly_channel_open(conn);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(mock_transport_written);
	mock_transport_written = false;

	firefly_protocol_channel_response res_pkt;
	res_pkt.source_chan_id = 1;
	res_pkt.dest_chan_id = channel_request.source_chan_id;
	res_pkt.ack = true;
	labcomm_encode_firefly_protocol_channel_response(test_enc, &res_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);
	ps.important = false;
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(mock_transport_acked);
	mock_transport_written = false;
	CU_ASSERT_TRUE(handshake_chan_open_called);

	handshake_chan_open_called = false;
	mock_transport_acked = false;
	mock_transport_written = false;
	firefly_connection_free(&conn);
}

void test_important_handshake_open_errors()
{
	struct firefly_connection_actions conn_actions = {
		.channel_recv		= important_handshake_chan_acc,
		.channel_opened		= important_handshake_chan_open,
		.channel_closed		= NULL,
		.channel_restrict	= NULL,
		.channel_restrict_info	= NULL
	};
	unsigned char *buf;
	size_t buf_size;
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = true, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = mock_transport_write_important,
		.ack = mock_transport_ack,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(&conn_actions, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);

	firefly_channel_open(conn);
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(mock_transport_written);
	mock_transport_written = false;

	firefly_protocol_channel_response res_pkt;
	res_pkt.source_chan_id = 1;
	res_pkt.dest_chan_id = channel_request.source_chan_id;
	res_pkt.ack = true;
	labcomm_encode_firefly_protocol_channel_response(test_enc, &res_pkt);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buf, &buf_size);
	protocol_data_received(conn, buf, buf_size);
	ps.important = false;
	event_execute_test(eq, 1);

	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_TRUE(mock_transport_acked);
	CU_ASSERT_TRUE(handshake_chan_open_called);
	mock_transport_written = false;
	mock_transport_acked = false;
	handshake_chan_open_called = false;

	protocol_data_received(conn, buf, buf_size);
	ps.important = false;
	event_execute_test(eq, 1);
	CU_ASSERT_TRUE(mock_transport_written);
	CU_ASSERT_FALSE(mock_transport_acked);
	CU_ASSERT_FALSE(handshake_chan_open_called);

	handshake_chan_open_called = false;
	mock_transport_acked = false;
	mock_transport_written = false;
	firefly_connection_free(&conn);
}

void test_important_ack_on_close()
{
	struct firefly_connection *conn;
	struct test_conn_platspec ps = { .important = false, .conn = &conn };
	struct firefly_transport_connection test_trsp_conn = {
		.write = mock_transport_write_important,
		.ack = mock_transport_ack,
		.open = test_conn_open,
		.close = NULL,
		.context = &ps
	};

	int res = firefly_connection_open(NULL, NULL, eq, &test_trsp_conn);
	CU_ASSERT_TRUE_FATAL(res > 0);
	event_execute_test(eq, 1);
	struct firefly_channel *chan = firefly_channel_new(conn);
	add_channel_to_connection(chan, conn);

	chan->important_id = TEST_IMPORTANT_ID;
	chan->current_seqno = 1;

	firefly_channel_close(chan);
	event_execute_all_test(eq);
	CU_ASSERT_TRUE(mock_transport_acked);

	mock_transport_written = false;
	mock_transport_acked = false;
	firefly_connection_free(&conn);
}
