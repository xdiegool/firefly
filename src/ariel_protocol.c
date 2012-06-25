#include "ariel_protocol.h"

struct ff_result* ariel_setup(struct ariel_connection *conn, unsigned char *buffer, size_t beat_rate, enum firefly_data_type type)
{
	struct ff_result *result = (struct ff_result *) malloc(sizeof(struct ff_result));
	if(conn == NULL) {
		result->type = FIREFLY_NULL_CONNECTION;
		result->msg = "Null ariel connection provided.";
		return result;
	}
	if (buffer == NULL) {
		result->type = FIREFLY_NULL_BUFFER;
		result->msg = "Null buffer provided.";
		return result;
	}
	if(beat_rate < 1 || beat_rate > 65535) { // 65536 requires more than 2 bytes to represent
		result->type = FIREFLY_INVALID_BEAT_RATE;
		result->msg = "Beat rate is invalid, must be 1 - 65535.";
		return result;
	}
	buffer[0] = FIRELFY_SETUP_ARIEL;
	buffer[1] = type;
	buffer[2] = (unsigned char) ((beat_rate >> 8) & 0xFF);
	buffer[3] = (unsigned char) ((beat_rate) & 0xFF);

	conn->beat_rate = beat_rate;
	conn->type = type;

	result->type = FIREFLY_RESULT_OK;
	return result;
}


static void handle_firefly_data(firefly_sample_data *samp, void *context)
{
	;
}

struct ff_result* ariel_setup_parse(struct ariel_connection *conn, unsigned char *buffer)
{
	struct ff_result *result = (struct ff_result *) malloc(sizeof(struct ff_result));
	if(conn == NULL) {
		result->type = FIREFLY_NULL_CONNECTION;
		result->msg = "Null ariel connection provided.";
		return result;
	}
	if (buffer == NULL) {
		result->type = FIREFLY_NULL_BUFFER;
		result->msg = "Null buffer provided.";
		return result;
	}
	
	conn->decoder_mcontext = (labcomm_mem_reader_context_t *) malloc(sizeof(labcomm_mem_reader_context_t));
	conn->decoder = labcomm_decoder_new(labcomm_mem_reader, conn->decoder_mcontext);
	labcomm_decoder_register_firefly_sample_data(conn->decoder, handle_firefly_data, NULL);

	conn->decoder_mcontext->enc_data = buffer + 1;
	conn->decoder_mcontext->size = FIREFLY_LABCOMM_SIGN_LEN;
	
	labcomm_decoder_decode_one(conn->decoder);


	result->type = FIREFLY_RESULT_OK;
	return result;
}
