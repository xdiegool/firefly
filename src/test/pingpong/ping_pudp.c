#include <pthread.h>
#include "test/pingpong/pingpong_pudp.h"

#include <labcomm.h>
#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_udp_posix.h>
#include <eventqueue/firefly_event_queue.h>

#include "gen/test.h"
#include "test/pingpong/hack_lctypes.h"

#define NBR_TESTS (6)

void *event_thread_main(void *args);
void *send_data(void *args);

void handle_test_var(test_test_var *var, void *ctx);

static char *ping_test_names[] = {
	"Open connection",
	"Open channel",
	"Send data",
	"Receive data",
	"Close channel",
	"Ping done"
};

static struct pingpong_test ping_tests[NBR_TESTS];

enum ping_test_id {
	CONNECTION_OPEN,
	CHAN_OPEN,
	DATA_SEND,
	DATA_RECEIVE,
	CHAN_CLOSE,
	TEST_DONE
};

void ping_init_tests()
{
	for (int i = 0; i < NBR_TESTS; i++) {
		pingpong_test_init(&ping_tests[i], ping_test_names[i]);
	}
}

void ping_pass_test(enum ping_test_id test_id)
{
	pingpong_test_pass(&ping_tests[test_id]);
}

/* Global event queue used by all connections. */
static struct firefly_event_queue *event_queue;
static pthread_mutex_t ping_done_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ping_done_signal = PTHREAD_COND_INITIALIZER;
static bool ping_done = false;

void chan_opened(struct firefly_channel *chan)
{
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
		fprintf(stderr, "channel_received: Could not create sender thread"
				" for channel.\n");
	}
	ping_pass_test(CHAN_OPEN);
}

void chan_closed(struct firefly_channel *chan)
{
	firefly_transport_connection_udp_posix_close(
			firefly_channel_get_connection(chan));
	pthread_mutex_lock(&ping_done_lock);
	ping_done = true;
	pthread_cond_signal(&ping_done_signal);
	pthread_mutex_unlock(&ping_done_lock);
	ping_pass_test(CHAN_CLOSE);
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
		struct firefly_transport_llp *llp, const char *ip_addr, unsigned short port)
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
	test_test_var data = PING_DATA;
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	labcomm_encode_test_test_var(enc, &data);
	ping_pass_test(DATA_SEND);
	return NULL;
}

void handle_test_var(test_test_var *var, void *ctx)
{
	if (*var == PONG_DATA) {
		ping_pass_test(DATA_RECEIVE);
	}
}

int main(int argc, char **argv)
{
	int res;
	pthread_t reader_thread;

	printf("Hello, Firefly from Ping!\n");
	ping_init_tests();
	struct firefly_transport_llp *llp =
			firefly_transport_llp_udp_posix_new(PING_PORT, connection_received);

	struct event_queue_signals eq_s;
	res = pthread_mutex_init(&eq_s.eq_lock, NULL);
	if (res) {
		fprintf(stderr, "ERROR: init mutex.\n");
	}
	res = pthread_cond_init(&eq_s.eq_cond, NULL);
	if (res) {
		fprintf(stderr, "ERROR: init cond variable.\n");
	}
	event_queue = firefly_event_queue_new(event_add_mutex, &eq_s);
	pthread_t event_thread;
	res = pthread_create(&event_thread, NULL, event_thread_main, event_queue);

	struct firefly_connection *conn =
			firefly_transport_connection_udp_posix_open(chan_opened, chan_closed,
					chan_received, event_queue, PONG_ADDR, PONG_PORT, llp);
	if (conn != NULL) {
		ping_pass_test(CONNECTION_OPEN);
	}
	hack_register_protocol_types(conn);

	firefly_channel_open(conn, channel_rejected);

	res = pthread_create(&reader_thread, NULL, reader_thread_main, llp);
	if (res) {
		fprintf(stderr, "ERROR: starting reader thread.\n");
	}

	pthread_mutex_lock(&ping_done_lock);
	while (!ping_done) {
		pthread_cond_wait(&ping_done_signal, &ping_done_lock);
	}
	pthread_mutex_unlock(&ping_done_lock);

	pthread_cancel(reader_thread);
	pthread_cancel(event_thread);

	pthread_join(reader_thread, NULL);
	pthread_join(event_thread, NULL);

	//firefly_transport_connection_udp_posix_free(&conn);

	firefly_event_queue_free(&event_queue);
	firefly_transport_llp_udp_posix_free(&llp);

	ping_pass_test(TEST_DONE);
	pingpong_test_print_results(ping_tests, NBR_TESTS);

	return 0;
}