#include "test/test_proto_protolc.h"

#include <stdbool.h>
#include <string.h>

#include "CUnit/Basic.h"
#include <labcomm.h>

#include <protocol/firefly_protocol.h>
#include <gen/test.h>
#include <gen/firefly_protocol.h>

#include "protocol/firefly_protocol_private.h"
#include "test/test_labcomm_utils.h"
#include "utils/cppmacros.h"


#define DATA_FILE	("testfiles/data.enc")	/* Encoded test data. */
#define SIG_FILE	("testfiles/sig.enc")	/* Encoded test signature. */
#define SRC_CHAN_ID	(3)			/* Source channe ID. */
#define DEST_CHAN_ID	(1)			/* Desination channel ID. */
#define WRITE_BUF_SIZE	(256)	/* Size of the LabComm buffer to write to. */

int init_suit_protolc()
{
	return 0;
}

int clean_suit_protolc()
{
	return 0;
}

static size_t proto_check_cnt = 0;

void proto_check_writer(unsigned char *data, size_t data_size,
			struct firefly_connection *conn, bool important, unsigned char *id)
{
	UNUSED_VAR(conn);
	UNUSED_VAR(important);
	UNUSED_VAR(id);
	if (proto_check_cnt == 0) {
		// DO nothing
	} else if (proto_check_cnt == 1 || proto_check_cnt == 2) {
		// Test sig.
		size_t file_size;
		unsigned char *file_data;
		file_size = read_file_to_mem(&file_data, DATA_FILE);

		CU_ASSERT_EQUAL(data_size, file_size);
		int cmp = memcmp(data, file_data, data_size);
		CU_ASSERT_EQUAL(0, cmp);
		if (cmp != 0) {
			printf("\nGOT\tEXP\tcnt:%zu\n", proto_check_cnt);
			for (size_t i = 0; i < file_size; i++) {
				printf("%02x\t%02x\n", data[i], file_data[i]);
			}
		}

		free(file_data);
	} else {
		CU_FAIL("You should not be here...\n");
	}
	++proto_check_cnt;
}

void test_proto_writer()
{
	struct firefly_event_queue *eq = firefly_event_queue_new(firefly_event_add,
			NULL);
	if (eq == NULL) {
		CU_FAIL("Could not create queue.\n");
	}
	// Prepare the expected data.
	test_test_var var_test_test = 42;
	create_labcomm_files_general(
		labcomm_encoder_register_test_test_var,
		(lc_encode_f) labcomm_encode_test_test_var,
		&var_test_test);
	unsigned char *test_sig_buf;
	unsigned char *test_data_buf;
	size_t proto_sig_size = read_file_to_mem(&test_sig_buf, SIG_FILE);
	size_t proto_data_size = read_file_to_mem(&test_data_buf, DATA_FILE);

	// Protocol packet for the sig.
	firefly_protocol_data_sample data_sample_sig;
	data_sample_sig.dest_chan_id = DEST_CHAN_ID;
	data_sample_sig.src_chan_id = SRC_CHAN_ID;
	data_sample_sig.seqno = 1;
	data_sample_sig.important = true;
	data_sample_sig.app_enc_data.a = test_sig_buf;
	data_sample_sig.app_enc_data.n_0 = proto_sig_size;

	// Protocol packet for the data.
	firefly_protocol_data_sample data_sample_data;
	data_sample_data.dest_chan_id = DEST_CHAN_ID;
	data_sample_data.src_chan_id = SRC_CHAN_ID;
	data_sample_data.seqno = 0;
	data_sample_data.important = false;
	data_sample_data.app_enc_data.a = test_data_buf;
	data_sample_data.app_enc_data.n_0 = proto_data_size;

	// Prepare for the test.
	struct firefly_connection conn;
	conn.transport_conn_platspec = NULL;
	conn.memory_replacements.alloc_replacement = NULL;
	conn.memory_replacements.free_replacement = NULL;
	conn.writer_data = malloc(sizeof(struct ff_transport_data));
	if (conn.writer_data == NULL) {
		CU_FAIL("Could not allocate conn.writer_data.");
	}
	conn.writer_data->data = malloc(WRITE_BUF_SIZE);
	if (conn.writer_data->data == NULL) {
		CU_FAIL("Could not allocate conn.writer_data->data.");
	}
	conn.writer_data->data_size = WRITE_BUF_SIZE;
	conn.writer_data->pos = 0;
	conn.reader_data = NULL;
	conn.transport_write = proto_check_writer;
	conn.event_queue = eq;
	conn.open = FIREFLY_CONNECTION_OPEN;

	struct labcomm_encoder *transport_encoder = labcomm_encoder_new(
				ff_transport_writer, &conn);
	if (transport_encoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_encoder(transport_encoder,
			handle_labcomm_error);
	labcomm_encoder_register_firefly_protocol_data_sample(transport_encoder);
	conn.transport_encoder = transport_encoder;

	struct firefly_channel chan;
	chan.conn = &conn;
	chan.local_id = SRC_CHAN_ID;
	chan.remote_id = DEST_CHAN_ID;
	chan.current_seqno = 0;
	chan.important_queue = NULL;
	chan.important_id = 0;
	struct ff_transport_data writer_data;
	writer_data.data = (unsigned char *) calloc(1, WRITE_BUF_SIZE);
	if (writer_data.data == NULL) {
		CU_FAIL("Could not alloc writebuf.\n");
	}
	writer_data.data_size = WRITE_BUF_SIZE;
	writer_data.pos = 0;
	chan.writer_data = &writer_data;
	struct labcomm_encoder *encoder_proto = labcomm_encoder_new(
			protocol_writer, &chan);
	if (encoder_proto == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_encoder(encoder_proto, handle_labcomm_error);
	chan.proto_encoder = encoder_proto;
	chan.proto_decoder = NULL;

	// Test signature packet.
	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_data_sample,
		(lc_encode_f) labcomm_encode_firefly_protocol_data_sample,
		&data_sample_sig);
	labcomm_encoder_register_test_test_var(encoder_proto);
	struct firefly_event *ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	// Test sample packet.
	create_labcomm_files_general(
		labcomm_encoder_register_firefly_protocol_data_sample,
		(lc_encode_f) labcomm_encode_firefly_protocol_data_sample,
		&data_sample_data);
	labcomm_encode_test_test_var(encoder_proto, &var_test_test);
	ev = firefly_event_pop(eq);
	CU_ASSERT_PTR_NOT_NULL(ev);
	firefly_event_execute(ev);

	CU_ASSERT_EQUAL(proto_check_cnt, 3);

	// Free all resources.
	labcomm_encoder_free(transport_encoder);
	free(writer_data.data);
	labcomm_encoder_free(encoder_proto);
	free(conn.writer_data->data);
	free(conn.writer_data);
	free(test_sig_buf);
	free(test_data_buf);
	firefly_event_queue_free(&eq);
}

static bool handled_test_var = false;
void handle_test_var(test_test_var *a_var, void *context)
{
	test_test_var *e_var = (test_test_var *) context;
	CU_ASSERT_EQUAL(*a_var, *e_var);
	handled_test_var = true;
}

void test_proto_reader()
{
	// Prepare the expected data.
	test_test_var var_test_test = 42;
	create_labcomm_files_general(
		labcomm_encoder_register_test_test_var,
		(lc_encode_f) labcomm_encode_test_test_var,
		&var_test_test);
	unsigned char *test_sig_buf;
	unsigned char *test_data_buf;
	size_t proto_sig_size = read_file_to_mem(&test_sig_buf, SIG_FILE);
	size_t proto_data_size = read_file_to_mem(&test_data_buf,
					DATA_FILE);

	struct ff_transport_data reader_data;
	struct firefly_channel chan;
	chan.reader_data = &reader_data;
	chan.reader_data->data = test_sig_buf;
	chan.reader_data->data_size = proto_sig_size;
	chan.reader_data->pos = 0;

	struct labcomm_decoder *proto_decoder = labcomm_decoder_new(
						protocol_reader, &chan);
	if (proto_decoder == NULL) {
		CU_FAIL("Could not allocate LabComm encoder or decoder.");
	}
	labcomm_register_error_handler_decoder(proto_decoder,
						handle_labcomm_error);
	labcomm_decoder_register_test_test_var(proto_decoder, handle_test_var,
							&var_test_test);

	labcomm_decoder_decode_one(proto_decoder);

	chan.reader_data->data = test_data_buf;
	chan.reader_data->data_size = proto_data_size;
	chan.reader_data->pos = 0;

	labcomm_decoder_decode_one(proto_decoder);

	CU_ASSERT_TRUE(handled_test_var);

	labcomm_decoder_free(proto_decoder);
	free(test_sig_buf);
	free(test_data_buf);
}

bool writer_important_written = false;
void proto_check_writer_signature(unsigned char *data, size_t data_size,
			struct firefly_connection *conn, bool important, unsigned char *id)
{
	UNUSED_VAR(conn);
	UNUSED_VAR(data);
	UNUSED_VAR(data_size);
	UNUSED_VAR(id);
	writer_important_written = important;
}
