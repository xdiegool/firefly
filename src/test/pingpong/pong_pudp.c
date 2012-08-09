#include <pthread.h>
#include "test/pingpong/pingpong_pudp.h"

#include <labcomm.h>
#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_udp_posix.h>
#include <eventqueue/event_queue.h>

#include "gen/test.h"

static struct firefly_event_queue *event_queue;
//TODO fix an esier way to close channel but to know the connection.
static struct firefly_connection *recv_conn = NULL;

void *send_data_and_close(void *args);

void chan_opened(struct firefly_channel *chan);
void chan_closed(struct firefly_channel *chan);
bool chan_received(struct firefly_channel *chan);

void handle_test_var(test_test_var *var, void *ctx);

struct firefly_connection *connection_received(
		struct firefly_transport_llp *llp, char *ip_addr, unsigned short port)
{
	/* If address is correct, open a connection. */
	if (strncmp(ip_addr, PING_ADDR, strlen(PING_ADDR)) == 0 &&
			port == PING_PORT) {
		recv_conn = firefly_transport_connection_udp_posix_open(
				chan_opened, chan_closed, chan_received, event_queue,
				ip_addr, port, llp);
	}
	return recv_conn;
}

void chan_opened(struct firefly_channel *chan)
{
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	struct labcomm_decoder *dec = firefly_protocol_get_input_stream(chan);

	labcomm_decoder_register_test_test_var(dec, handle_test_var, chan);
	labcomm_encoder_register_test_test_var(enc);
}

void chan_closed(struct firefly_channel *chan)
{
	firefly_transport_connection_udp_posix_free(&recv_conn);
}

bool chan_received(struct firefly_channel *chan)
{
	return true;
}

void channel_rejected(struct firefly_connection *conn)
{
	printf("Channel rejected\n");
}

void handle_test_var(test_test_var *var, void *ctx)
{
	pthread_t sender;
	pthread_attr_t tattr;
	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	int err = pthread_create(&sender, &tattr, send_data_and_close, ctx);
	if (err) {
		fprintf(stderr, "channel_received: Could not create sender thread for channel.\n");
	}
}

void *send_data_and_close(void *args)
{
	struct firefly_channel *chan = (struct firefly_channel *) args;
	printf("Sending data on channel: %p.\n", chan);
	test_test_var data = 1111;
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	labcomm_encode_test_test_var(enc, &data);
	printf("Closing channel\n");
	firefly_channel_close(chan, recv_conn);
	return NULL;
}

int main(int argc, char **argv)
{
	int res;
	printf("Hello, Firefly from Pong!\n");
	event_queue = firefly_event_queue_new(firefly_event_add);
	struct event_queue_signals eq_s;
	res = pthread_mutex_init(&eq_s.eq_lock, NULL);
	if (res) {
		fprintf(stderr, "ERROR: init mutex.\n");
	}
	res = pthread_cond_init(&eq_s.eq_cond, NULL);
	if (res) {
		fprintf(stderr, "ERROR: init cond variable.\n");
	}
	event_queue->context = &eq_s;
	pthread_t event_thread;
	res = pthread_create(&event_thread, NULL, event_thread_main, event_queue);
	struct firefly_transport_llp *llp =
			firefly_transport_llp_udp_posix_new(PONG_PORT, connection_received);

	reader_thread_main(llp);

	firefly_transport_llp_udp_posix_free(&llp);
}
