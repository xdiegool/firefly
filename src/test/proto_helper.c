#include "test/proto_helper.h"

#include <stdlib.h>
#include <labcomm.h>
#include <labcomm_mem_writer.h>
#include <labcomm_mem_reader.h>
#include <labcomm_fd_reader_writer.h>
#include "CUnit/Basic.h"
#include <stdbool.h>

#include <gen/firefly_protocol.h>
#include <gen/test.h>
#include <firefly_errors.h>
#include <eventqueue/firefly_event_queue.h>

#include "test/test_labcomm_utils.h"

labcomm_mem_reader_context_t *test_dec_ctx = NULL;
struct labcomm_decoder *test_dec = NULL;
struct tmp_data conn_open_write;
struct tmp_data conn_recv_write;
bool conn_recv_accept_called = false;

struct firefly_connection *setup_test_conn_new(firefly_channel_is_open_f ch_op,
		firefly_channel_closed_f ch_cl, firefly_channel_accept_f ch_acc,
		struct firefly_event_queue *eq)
{
	struct firefly_connection *conn =
		firefly_connection_new(ch_op, ch_cl, ch_acc,
				transport_write_test_decoder, eq, NULL);
	if (conn == NULL) {
		CU_FAIL("Could not create connection.\n");
	}
	// Set some stuff that firefly_init_conn doesn't do
	labcomm_register_error_handler_encoder(conn->transport_encoder,
			handle_labcomm_error);
	labcomm_register_error_handler_decoder(conn->transport_decoder,
			handle_labcomm_error);
	return conn;
}

bool chan_opened_called = false;
void chan_opened_mock(struct firefly_channel *chan)
{
	chan_opened_called = true;
}

void transport_write_test_decoder(unsigned char *data, size_t size,
								  struct firefly_connection *conn)
{
	test_dec_ctx->enc_data = data;
	test_dec_ctx->size = size;
	labcomm_decoder_decode_one(test_dec);
	test_dec_ctx->enc_data = NULL;
	test_dec_ctx->size = 0;
}

bool chan_open_recv_accept_open(struct firefly_channel *chan)
{
	CU_FAIL("Connection open received channel.");
	return false;
}

void chan_open_recv_write_open(unsigned char *data, size_t size,
							   struct firefly_connection *conn)
{
	conn_open_write.data = malloc(size);
	memcpy(conn_open_write.data, data, size);
	conn_open_write.size = size;
}

bool chan_open_recv_accept_recv(struct firefly_channel *chan)
{
	return conn_recv_accept_called = true;
}

void chan_open_recv_write_recv(unsigned char *data, size_t size,
							   struct firefly_connection *conn)
{
	conn_recv_write.data = malloc(size);
	memcpy(conn_recv_write.data, data, size);
	conn_recv_write.size = size;
}

void free_tmp_data(struct tmp_data *td)
{
	free(td->data);
	td->data = NULL;
	td->size = 0;
}
