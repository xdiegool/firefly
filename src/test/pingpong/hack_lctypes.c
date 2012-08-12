#include "test/pingpong/hack_lctypes.h"

#include <protocol/firefly_protocol.h>
#include "protocol/firefly_protocol_private.h"
#include "gen/firefly_protocol.h"

void hack_write_signatures(unsigned char *data, size_t size,
		struct firefly_connection *conn)
{
	protocol_data_received(conn, data, size);
}

void hack_register_protocol_types(struct firefly_connection *conn)
{
	struct labcomm_encoder *enc = conn->transport_encoder;
	struct labcomm_decoder *dec = conn->transport_decoder;
	labcomm_decoder_register_firefly_protocol_channel_request(dec,
			handle_channel_request, conn);
	labcomm_decoder_register_firefly_protocol_channel_response(dec,
			handle_channel_response, conn);
	labcomm_decoder_register_firefly_protocol_channel_ack(dec,
			handle_channel_ack, conn);
	labcomm_decoder_register_firefly_protocol_channel_close(dec,
			handle_channel_close, conn);
	labcomm_decoder_register_firefly_protocol_data_sample(dec,
			handle_data_sample, conn);

	transport_write_f tmp_writer = conn->transport_write;
	conn->transport_write = hack_write_signatures;
	
	labcomm_encoder_register_firefly_protocol_channel_request(enc);
	labcomm_encoder_register_firefly_protocol_channel_response(enc);
	labcomm_encoder_register_firefly_protocol_channel_ack(enc);
	labcomm_encoder_register_firefly_protocol_channel_close(enc);
	labcomm_encoder_register_firefly_protocol_data_sample(enc);

	conn->transport_write = tmp_writer;
}
