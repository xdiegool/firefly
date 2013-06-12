#include <stdio.h>
#include <CUnit/Basic.h>
#include "CUnit/Console.h"

#include <labcomm.h>
#include <labcomm_ioctl.h>
#include <labcomm_static_buffer_writer.h>
#include <labcomm_static_buffer_reader.h>
#include <utils/cppmacros.h>
#include "gen/test.h"
#include "test/proto_helper.h"
#include "test/event_helper.h"

struct memman_list {
	void *p;
	bool runtime;
	struct memman_list *next;
};

struct memory_management_stats {
	int pre_malloc;
	int pre_realloc;
	int pre_free;
	size_t pre_size;
	int dirty_malloc;
	int dirty_realloc;
	int dirty_free;
	size_t dirty_size;
	int clean_malloc;
	int clean_realloc;
	int clean_free;
	size_t clean_size;
	int runtime_malloc;
	int runtime_free;
	size_t runtime_size;
};

extern struct labcomm_decoder *test_dec;

extern struct labcomm_encoder *test_enc;

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
static bool memtest_started = false;

void *__real_malloc(size_t s);
void *__real_realloc(void *ptr, size_t s);
void __real_free(void *ptr);

static struct memory_management_stats memman_stats;

static struct memman_list *mem_allocs = NULL;

void memman_list_add(void *p, bool runtime)
{
	struct memman_list *new_mem = __real_malloc(sizeof(*new_mem));
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
			__real_free(tmp);
			found = true;
			break;
		} else {
			sought = &(*sought)->next;
		}
	}
}

void *__wrap_malloc(size_t s)
{
	if (memtest_started) {
		memman_stats.dirty_malloc++;
		memman_stats.dirty_size += s;
	} else {
		memman_stats.pre_malloc++;
		memman_stats.pre_size += s;
	}
	return __real_malloc(s);
}

void *__wrap_realloc(void *p, size_t s)
{
	if (memtest_started) {
		memman_stats.dirty_realloc++;
		memman_stats.dirty_size += s;
	} else {
		memman_stats.pre_realloc++;
		memman_stats.pre_size += s;
	}
	return __real_realloc(p, s);
}

void __wrap_free(void *p)
{
	if (memtest_started) {
		memman_stats.dirty_free++;
	} else {
		memman_stats.pre_free++;
	}
	__real_free(p);
}

void *test_malloc(size_t size)
{
	void *p = __real_malloc(size);
	if (memtest_started) {
		memman_stats.clean_malloc++;
		memman_stats.clean_size += size;
		memman_list_add(p, false);
		CU_ASSERT_FALSE(firefly_runtime);
		if (firefly_runtime) {
			CU_FAIL("ERROR: malloc during runtime\n");
		}
	} else {
		memman_stats.pre_malloc++;
		memman_stats.pre_size += size;
	}
	return p;
}

void *test_realloc(void *ptr, size_t size)
{
	void *p = __real_realloc(ptr, size);
	if (memtest_started) {
		memman_stats.clean_realloc++;
		memman_stats.clean_size += size;
		memman_list_remove(ptr, false);
		memman_list_add(p, false);
		CU_ASSERT_FALSE(firefly_runtime);
		if (firefly_runtime) {
			CU_FAIL("ERROR: realloc during runtime\n");
		}
	} else {
		memman_stats.pre_realloc++;
		memman_stats.pre_size += size;
	}
	return p;
}

void test_free(void *p)
{
	if (memtest_started) {
		if (p != NULL) {
			memman_stats.clean_free++;
			memman_list_remove(p, false);
		}
	} else {
		memman_stats.pre_free++;
	}
	__real_free(p);
}

void *test_runtime_malloc(struct firefly_connection *conn, size_t size)
{
	memman_stats.runtime_malloc++;
	memman_stats.runtime_size += size;
	UNUSED_VAR(conn);
	void *p = __real_malloc(size);
	memman_list_add(p, true);
	return p;
}

void test_runtime_free(struct firefly_connection *conn, void *p)
{
	memman_stats.runtime_free++;
	UNUSED_VAR(conn);
	if (p != NULL) {
		memman_list_remove(p, true);
	}
	__real_free(p);
}

void *test_reader_runtime_malloc(void *c, size_t size)
{
	return test_runtime_malloc(c, size);
}

void test_reader_runtime_free(void *c, void *p)
{
	test_runtime_free(c, p);
}

int init_suit_proto_memman()
{
	struct labcomm_writer *test_w = labcomm_static_buffer_writer_new();
	struct labcomm_reader *test_r = labcomm_static_buffer_reader_mem_new(
			NULL, test_reader_runtime_malloc, test_reader_runtime_free);
	init_labcomm_test_enc_dec_custom(test_r, test_w);
	return 0;
}

int clean_suit_proto_memman()
{
	clean_labcomm_test_enc_dec();
	return 0;
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
	memtest_started = true;

	void *buffer;
	size_t buffer_size;
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
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buffer, &buffer_size);
	protocol_data_received(conn, buffer, buffer_size);
	event_execute_all_test(eq);
	CU_ASSERT_TRUE_FATAL(received_channel_ack);
	CU_ASSERT_PTR_NOT_NULL_FATAL(open_chan);

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
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buffer, &buffer_size);
	protocol_data_received(conn, buffer, buffer_size);
	event_execute_all_test(eq);
	CU_ASSERT_TRUE_FATAL(received_ack);
	received_ack = false;

	ack.src_chan_id = open_chan->remote_id;
	ack.dest_chan_id = open_chan->local_id;
	labcomm_encode_firefly_protocol_ack(test_enc, &ack);
	CU_ASSERT_NOT_EQUAL(open_chan->important_id, 0);
	labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
			&buffer, &buffer_size);
	protocol_data_received(conn, buffer, buffer_size);
	event_execute_all_test(eq);
	CU_ASSERT_EQUAL(open_chan->important_id, 0);

	// --- END SETUP PHASE
	// --- RUNTIME PHASE

	firefly_runtime = true;

	test_test_var var;
	for (int i = 1; i < 100; i++) {
		var = i;
		labcomm_encode_test_test_var(chan_out, &var);
		event_execute_all_test(eq);
		CU_ASSERT_TRUE_FATAL(received_data_sample);
		received_data_sample = false;
		data_sample.src_chan_id = open_chan->remote_id;
		data_sample.dest_chan_id = open_chan->local_id;
		labcomm_encode_firefly_protocol_data_sample(test_enc, &data_sample);
		labcomm_encoder_ioctl(test_enc, LABCOMM_IOCTL_WRITER_GET_BUFFER,
				&buffer, &buffer_size);
		protocol_data_received(conn, buffer, buffer_size);
		event_execute_all_test(eq);
	}

	firefly_runtime = false;

	firefly_connection_free(&conn);
	event_execute_all_test(eq);
	firefly_event_queue_free(&eq);

	if (memman_stats.clean_free == 0|| memman_stats.clean_malloc == 0) {
		CU_FAIL("compile with:\nLABCOMM_MALLOC=test_malloc(size) LABCOMM_FREE=test_free(p) FIREFLY_MALLOC=test_malloc(size) FIREFLY_FREE=test_free(p)\n");
	}
	CU_ASSERT_EQUAL(memman_stats.dirty_malloc, 0);
	char *stats_format = "\t%d\t%d\t\t%d\t%zu\n";
	printf("Memory management statistics:\n");
	printf("\t\tMallocs\tReallocs\tFrees\tAccumulated size\n");
	printf("Outside tets:");
	printf(stats_format,
			memman_stats.pre_malloc, memman_stats.pre_realloc,
			memman_stats.pre_free, memman_stats.pre_size);
	printf("Dirty:\t");
	printf(stats_format,
			memman_stats.dirty_malloc, memman_stats.dirty_realloc,
			memman_stats.dirty_free, memman_stats.dirty_size);
	printf("Clean:\t");
	printf(stats_format,
			memman_stats.clean_malloc, memman_stats.clean_realloc,
			memman_stats.clean_free, memman_stats.clean_size);
	printf("Runtime:");
	printf(stats_format,
			memman_stats.runtime_malloc, 0,
			memman_stats.runtime_free, memman_stats.runtime_size);
	memtest_started = false;
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
