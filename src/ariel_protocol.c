#include "ariel_protocol.h"

struct ff_result* ariel_setup(struct ariel_connection *conn, unsigned char *buffer, size_t beat_rate, enum firefly_data_type type)
{
	struct ff_result *result = (struct ff_result *) malloc(sizeof(struct ff_result));
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

struct ff_result* ariel_setup_parse(struct ariel_connection *conn, unsigned char *buffer)
{
	return NULL;
}
