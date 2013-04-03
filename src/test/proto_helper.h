#ifndef PROTO_HELPER_H
#define PROTO_HELPER_H

#include "protocol/firefly_protocol_private.h"
#include <protocol/firefly_protocol.h>

struct tmp_data {
	unsigned char *data;
	size_t size;
};

struct firefly_connection *setup_test_conn_new(firefly_channel_is_open_f ch_op,
		firefly_channel_closed_f ch_cl, firefly_channel_accept_f ch_acc,
		struct firefly_event_queue *eq);

void chan_opened_mock(struct firefly_channel *chan);

void transport_write_test_decoder(unsigned char *data, size_t size,
					  struct firefly_connection *conn, bool important,
					   unsigned char *id);

bool chan_open_recv_accept_open(struct firefly_channel *chan);

void chan_open_recv_write_open(unsigned char *data, size_t size,
							   struct firefly_connection *conn, bool important,
					   unsigned char *id);

bool chan_open_recv_accept_recv(struct firefly_channel *chan);

void chan_open_recv_write_recv(unsigned char *data, size_t size,
					   struct firefly_connection *conn, bool important,
					   unsigned char *id);

void free_tmp_data(struct tmp_data *td);

#endif
