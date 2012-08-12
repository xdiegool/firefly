#include <stdbool.h>

#include "test/test_labcomm_utils.h"
#include "gen/firefly_protocol.h"

int main(int argc, char **argv)
{
	firefly_protocol_channel_request chan_req;
	chan_req.source_chan_id = 0;
	chan_req.dest_chan_id = 1;
	create_lc_files_name(
			labcomm_encoder_register_firefly_protocol_channel_request,
			(lc_encode_f) labcomm_encode_firefly_protocol_channel_request,
			&chan_req, "chan_req");

	firefly_protocol_channel_response chan_res;
	chan_res.source_chan_id = 0;
	chan_res.dest_chan_id = 1;
	chan_res.ack = true;
	create_lc_files_name(
			labcomm_encoder_register_firefly_protocol_channel_response,
			(lc_encode_f) labcomm_encode_firefly_protocol_channel_response,
			&chan_res, "chan_res");

	firefly_protocol_channel_ack chan_ack;
	chan_ack.source_chan_id = 0;
	chan_ack.dest_chan_id = 1;
	chan_ack.ack = true;
	create_lc_files_name(
			labcomm_encoder_register_firefly_protocol_channel_ack,
			(lc_encode_f) labcomm_encode_firefly_protocol_channel_ack,
			&chan_ack, "chan_ack");

	firefly_protocol_channel_close chan_close;
	chan_close.source_chan_id = 0;
	chan_close.dest_chan_id = 1;
	create_lc_files_name(
			labcomm_encoder_register_firefly_protocol_channel_close,
			(lc_encode_f) labcomm_encode_firefly_protocol_channel_close,
			&chan_close, "chan_close");

	firefly_protocol_data_sample sample;
	sample.src_chan_id = 0;
	sample.dest_chan_id = 1;
	sample.app_enc_data.n_0 = 0;
	sample.app_enc_data.a = NULL;
	create_lc_files_name(
			labcomm_encoder_register_firefly_protocol_data_sample,
			(lc_encode_f) labcomm_encode_firefly_protocol_data_sample,
			&sample, "sample");
}
