#include <CUnit/Basic.h>
#include "CUnit/Console.h"

#include <labcomm.h>
#include <test/labcomm_mem_writer.h>
#include <test/labcomm_mem_reader.h>
#include <utils/cppmacros.h>
#include "gen/test.h"
#include "test/proto_helper.h"
#include "test/event_helper.h"

struct memman_list {
	void *p;
	bool runtime;
	struct memman_list *next;
};

extern struct labcomm_decoder *test_dec;
extern labcomm_mem_reader_context_t *test_dec_ctx;

extern struct labcomm_encoder *test_enc;
extern labcomm_mem_writer_context_t *test_enc_ctx;

extern firefly_protocol_data_sample data_sample;
extern firefly_protocol_ack ack;
extern firefly_protocol_channel_request channel_request;
extern firefly_protocol_channel_response channel_response;
extern firefly_protocol_channel_ack channel_ack;
extern bool received_data_sample;
extern bool received_channel_request;
extern bool received_channel_response;
extern bool received_channel_ack;
extern bool received_channel_close;
extern bool received_ack;

struct firefly_event_queue *eq;
static bool firefly_runtime = false;

static struct memman_list *mem_allocs = NULL;

void memman_list_add(void *p, bool runtime)
{
	struct memman_list *new_mem = malloc(sizeof(*new_mem));
	new_mem->p = p;
	new_mem->runtime = runtime;
	new_mem->next = NULL;
	struct memman_list **last = &mem_allocs;
	while (*last != NULL) {
		last = &(*last)->next;
	}
	*last = new_mem;
}

void memman_list_remove(void *p, bool runtime)
{
	struct memman_list **sought = &mem_allocs;
	bool found = false;
	while (*sought != NULL) {
		if ((*sought)->p == p) {
			if ((*sought)->runtime != runtime) {
				printf("ERROR: freeing from incorrent context: %d\n", runtime);
			}
			struct memman_list *tmp = *sought;
			*sought = (*sought)->next;
			free(tmp);
			found = true;
			break;
		} else {
			sought = &(*sought)->next;
		}
	}
}

int init_suit_proto_memman()
{
	init_labcomm_test_enc_dec();
	return 0;
}

int clean_suit_proto_memman()
{
	clean_labcomm_test_enc_dec();
	return 0;
}

static int nbr_mallocs = 0;
void *test_malloc(size_t size)
{
	nbr_mallocs++;
	void *p = malloc(size);
	memman_list_add(p, false);
	CU_ASSERT_FALSE(firefly_runtime);
	if (firefly_runtime) {
		printf("ERROR: malloc during runtime\n");
		return NULL;
	}
	return p;
}

static int nbr_frees = 0;
void test_free(void *p)
{
	nbr_frees++;
	if (p != NULL) {
		memman_list_remove(p, false);
	}
	free(p);
}

void *test_runtime_malloc(struct firefly_connection *conn, size_t size)
{
	nbr_mallocs++;
	UNUSED_VAR(conn);
	void *p = malloc(size);
	memman_list_add(p, true);
	return p;
}

void test_runtime_free(struct firefly_connection *conn, void *p)
{
	nbr_frees++;
	UNUSED_VAR(conn);
	if (p != NULL) {
		memman_list_remove(p, true);
	}
	free(p);
}

static struct firefly_channel *open_chan = NULL;
void memman_chan_open(struct firefly_channel *chan)
{
	open_chan = chan;
}
bool memman_chan_accept(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	return false;
}
void memman_chan_closed(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	open_chan = NULL;
}

void handle_test_test_var(test_test_var *v, void *ctx)
{
	UNUSED_VAR(v);
	UNUSED_VAR(ctx);
}

void test_memory_management_one_chan()
{
	eq = firefly_event_queue_new(firefly_event_add, 10, NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(eq);
	struct firefly_memory_funcs memfuncs;
	memfuncs.alloc_replacement = test_runtime_malloc;
	memfuncs.free_replacement = test_runtime_free;
	struct firefly_connection *conn = firefly_connection_new(
			memman_chan_open, memman_chan_closed, memman_chan_accept,
			transport_write_test_decoder, transport_ack_test,
			&memfuncs, eq, NULL, NULL);
	// --- SETUP PHASE ---
	firefly_channel_open(conn, NULL);
	event_execute_all_test(eq);
	CU_ASSERT_TRUE_FATAL(received_channel_request);
	firefly_protocol_channel_response resp;
	resp.source_chan_id = 1;
	resp.dest_chan_id = channel_request.source_chan_id;
	resp.ack = true;
	labcomm_encode_firefly_protocol_channel_response(test_enc, &resp);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	event_execute_all_test(eq);
	CU_ASSERT_TRUE_FATAL(received_channel_ack);
	CU_ASSERT_PTR_NOT_NULL_FATAL(open_chan);

	// --- END SETUP PHASE
	// --- RUNTIME PHASE
	firefly_runtime = true;

	struct labcomm_encoder *chan_out =
		firefly_protocol_get_output_stream(open_chan);

	labcomm_decoder_register_test_test_var(
			firefly_protocol_get_input_stream(open_chan),
			handle_test_test_var, NULL);

	labcomm_encoder_register_test_test_var(chan_out);
	event_execute_all_test(eq);
	CU_ASSERT_TRUE(received_data_sample);
	received_data_sample = false;

	data_sample.src_chan_id = open_chan->remote_id;
	data_sample.dest_chan_id = open_chan->local_id;
	labcomm_encode_firefly_protocol_data_sample(test_enc, &data_sample);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	test_enc_ctx->write_pos = 0;
	event_execute_all_test(eq);
	CU_ASSERT_TRUE_FATAL(received_ack);
	received_ack = false;


	ack.src_chan_id = open_chan->remote_id;
	ack.dest_chan_id = open_chan->local_id;
	labcomm_encode_firefly_protocol_ack(test_enc, &ack);
	CU_ASSERT_NOT_EQUAL(open_chan->important_id, 0);
	protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
	event_execute_all_test(eq);
	test_enc_ctx->write_pos = 0;
	CU_ASSERT_EQUAL(open_chan->important_id, 0);

	test_test_var var;
	for (int i = 1; i < 10; i++) {
		var = i;
		labcomm_encode_test_test_var(chan_out, &var);
		event_execute_all_test(eq);
		CU_ASSERT_TRUE_FATAL(received_data_sample);
		received_data_sample = false;
		data_sample.src_chan_id = open_chan->remote_id;
		data_sample.dest_chan_id = open_chan->local_id;
		labcomm_encode_firefly_protocol_data_sample(test_enc, &data_sample);
		protocol_data_received(conn, test_enc_ctx->buf, test_enc_ctx->write_pos);
		test_enc_ctx->write_pos = 0;
		event_execute_all_test(eq);
	}

	firefly_runtime = false;

	firefly_connection_free(&conn);
	event_execute_all_test(eq);
	firefly_event_queue_free(&eq);
	printf("\nnbr mallocs: %d\nnbr frees: %d\n", nbr_mallocs, nbr_frees);

	if (nbr_frees == 0|| nbr_mallocs == 0) {
		printf("compile with:\n");
		printf("FIREFLY_MALLOC=test_malloc(size) FIREFLY_FREE=test_free(p)\n");
	}
	CU_ASSERT_NOT_EQUAL(nbr_mallocs, 0);
	CU_ASSERT_NOT_EQUAL(nbr_frees, 0);
}

int main()
{
	CU_pSuite ff_memman = NULL;

	// Initialize CUnit test registry.
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	ff_memman = CU_add_suite("ff_memman", init_suit_proto_memman, clean_suit_proto_memman);
	if (ff_memman == NULL) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/*Transport general tests.*/
	if (
		(CU_add_test(ff_memman, "test_memory_management_one_chan",
				test_memory_management_one_chan) == NULL)
	) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Set verbosity.
	CU_basic_set_mode(CU_BRM_VERBOSE);
	/*CU_console_run_tests();*/

	// Run all test suites.
	CU_basic_run_tests();
	int res = CU_get_number_of_tests_failed();
	// Clean up.
	CU_cleanup_registry();

	if (res != 0) {
		return 1;
	}
}
