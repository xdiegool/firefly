#ifndef ARIEL_PROTOCOL_H
#define ARIEL_PROTOCOL_H
#include <stdlib.h>
#include "firefly_protocol.h"
#include <labcomm.h>
#include <gen/firefly_sample.h>
#include <labcomm_mem_reader.h>

// TODO move struct to private h file.
struct ariel_connection {
	size_t beat_rate;
	enum firefly_data_type type;
	struct labcomm_decoder *decoder;
	labcomm_mem_reader_context_t *decoder_mcontext;
};

struct ff_result* ariel_setup(struct ariel_connection *conn, unsigned char *buffer, size_t beat_rate, enum firefly_data_type type);

struct ff_result* ariel_setup_parse(struct ariel_connection *conn, unsigned char *buffer);

#endif
