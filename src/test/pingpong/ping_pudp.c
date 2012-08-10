#include <pthread.h>
#include "test/pingpong/pingpong_pudp.h"

#include <labcomm.h>
#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_udp_posix.h>
#include <eventqueue/event_queue.h>

#include "gen/test.h"
#include "test/pingpong/hack_lctypes.h"

void *event_thread_main(void *args);
void *send_data(void *args);

void handle_test_var(test_test_var *var, void *ctx);

/* Global event queue used by all connections. */
static struct firefly_event_queue *event_queue;

void chan_opened(struct firefly_channel *chan)
{
	printf("PONG: Channel opened.\n");
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	struct labcomm_decoder *dec = firefly_protocol_get_input_stream(chan);

	labcomm_decoder_register_test_test_var(dec, handle_test_var, chan);
	labcomm_encoder_register_test_test_var(enc);

	pthread_t sender;
	pthread_attr_t tattr;
	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	int err = pthread_create(&sender, &tattr, send_data, chan);
	if (err) {
		fprintf(stderr, "channel_received: Could not create sender thread for channel.\n");
	}
}

void chan_closed(struct firefly_channel *chan)
{
	printf("PING: Channel closed\n");
}

bool chan_received(struct firefly_channel *chan)
{
	printf("PING: Channel received\n");
	return true;
}

void channel_rejected(struct firefly_connection *conn)
{
	fprintf(stderr, "ERROR: Channel was rejected.\n");
}

struct firefly_connection *connection_received(
		struct firefly_transport_llp *llp, char *ip_addr, unsigned short port)
{
	printf("PING: Connection received.\n");
	struct firefly_connection *conn = NULL;
	/* If address is correct, open a connection. */
	if (strncmp(ip_addr, PONG_ADDR, strlen(PONG_ADDR)) == 0 &&
			port == PONG_PORT) {
		printf("PING: Connection accepted.\n");
		conn = firefly_transport_connection_udp_posix_open(
				chan_opened, chan_closed, chan_received, event_queue,
				ip_addr, port, llp);
		hack_register_protocol_types(conn);
	}
	return conn;
}

void *send_data(void *args)
{
	struct firefly_channel *chan = (struct firefly_channel *) args;
	printf("Sending data on channel: %p.\n", chan);
	test_test_var data = 1111;
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	labcomm_encode_test_test_var(enc, &data);
	return NULL;
}

void handle_test_var(test_test_var *var, void *ctx)
{
	printf("Got data\n");
}

int main(int argc, char **argv)
{
	printf("Hello, Firefly from Ping!\n");
	int res;
	struct firefly_transport_llp *llp =
			firefly_transport_llp_udp_posix_new(PING_PORT, connection_received);

	event_queue = firefly_event_queue_new(event_add_mutex);
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
	printf("Starting event thread.\n");
	res = pthread_create(&event_thread, NULL, event_thread_main, event_queue);

	printf("Open connection.\n");
	struct firefly_connection *conn =
			firefly_transport_connection_udp_posix_open(chan_opened, chan_closed,
					chan_received, event_queue, PONG_ADDR, PONG_PORT, llp);
	printf("Registering types.\n");
	hack_register_protocol_types(conn);

	printf("Open channel.\n");
	firefly_channel_open(conn, channel_rejected);

	reader_thread_main(llp);

	firefly_transport_connection_udp_posix_free(&conn);

	firefly_transport_llp_udp_posix_free(&llp);

	return 0;
}
