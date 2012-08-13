#include <pthread.h>
#include "test/pingpong/pingpong_pudp.h"

#include <labcomm.h>
#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_udp_posix.h>
#include <eventqueue/event_queue.h>

#include "gen/test.h"
#include "test/pingpong/hack_lctypes.h"

#define NBR_TESTS (7)

static char *pong_test_names[] = {
	"Open connection",
	"Received channel",
	"Open channel",
	"Send data",
	"Receive data",
	"Close channel",
	"Ping done"
};

static struct pingpong_test pong_tests[NBR_TESTS];

enum pong_test_id {
	CONNECTION_OPEN,
	CHAN_RECEIVE,
	CHAN_OPEN,
	DATA_SEND,
	DATA_RECEIVE,
	CHAN_CLOSE,
	TEST_DONE
};

void pong_init_tests()
{
	for (int i = 0; i < NBR_TESTS; i++) {
		pingpong_test_init(&pong_tests[i], pong_test_names[i]);
	}
}

void pong_pass_test(enum pong_test_id test_id)
{
	pingpong_test_pass(&pong_tests[test_id]);
}

static struct firefly_event_queue *event_queue;
static struct firefly_connection *recv_conn = NULL;
static pthread_mutex_t pong_done_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pong_done_signal = PTHREAD_COND_INITIALIZER;
static bool pong_done = false;

void chan_opened(struct firefly_channel *chan);
void chan_closed(struct firefly_channel *chan);
bool chan_received(struct firefly_channel *chan);
void handle_test_var(test_test_var *var, void *ctx);
void *send_data_and_close(void *args);

struct firefly_connection *connection_received(
		struct firefly_transport_llp *llp, char *ip_addr, unsigned short port)
{
	/* If address is correct, open a connection. */
	if (strncmp(ip_addr, PING_ADDR, strlen(PING_ADDR)) == 0 &&
			port == PING_PORT) {
		recv_conn = firefly_transport_connection_udp_posix_open(
				chan_opened, chan_closed, chan_received, event_queue,
				ip_addr, port, llp);
		hack_register_protocol_types(recv_conn);
		pong_pass_test(CONNECTION_OPEN);
	} else {
		fprintf(stderr, "ERROR: Received unknown connection: %s:%hu\n",
				ip_addr, port);
	}
	return recv_conn;
}

void chan_opened(struct firefly_channel *chan)
{
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	struct labcomm_decoder *dec = firefly_protocol_get_input_stream(chan);

	labcomm_decoder_register_test_test_var(dec, handle_test_var, chan);
	labcomm_encoder_register_test_test_var(enc);
	pong_pass_test(CHAN_OPEN);
}

void chan_closed(struct firefly_channel *chan)
{
	//firefly_transport_connection_udp_posix_free(&recv_conn);
	pthread_mutex_lock(&pong_done_lock);
	pong_done = true;
	pthread_cond_signal(&pong_done_signal);
	pong_pass_test(CHAN_CLOSE);
	pthread_mutex_unlock(&pong_done_lock);
}

bool chan_received(struct firefly_channel *chan)
{
	pong_pass_test(CHAN_RECEIVE);
	return true;
}

void channel_rejected(struct firefly_connection *conn)
{
	fprintf(stderr, "ERROR: Channel rejected.\n");
}

void handle_test_var(test_test_var *var, void *ctx)
{
	pthread_t sender;
	pthread_attr_t tattr;
	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	int err = pthread_create(&sender, &tattr, send_data_and_close, ctx);
	if (err) {
		fprintf(stderr, "channel_received: Could not create sender thread"
				" for channel.\n");
	}
	pong_pass_test(DATA_RECEIVE);
}

void *send_data_and_close(void *args)
{
	struct firefly_channel *chan = (struct firefly_channel *) args;
	test_test_var data = PONG_DATA;
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	labcomm_encode_test_test_var(enc, &data);
	pong_pass_test(DATA_SEND);
	firefly_channel_close(chan);

	return NULL;
}

int main(int argc, char **argv)
{
	int res;
	pthread_t event_thread;
	pthread_t reader_thread;

	printf("Hello, Firefly from Pong!\n");
	pong_init_tests();
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
	res = pthread_create(&event_thread, NULL, event_thread_main, event_queue);
	if (res) {
		fprintf(stderr, "ERROR: starting event thread.\n");
	}

	struct firefly_transport_llp *llp =
			firefly_transport_llp_udp_posix_new(PONG_PORT, connection_received);

	res = pthread_create(&reader_thread, NULL, reader_thread_main, llp);
	if (res) {
		fprintf(stderr, "ERROR: starting reader thread.\n");
	}

	pthread_mutex_lock(&pong_done_lock);
	while (!pong_done) {
		pthread_cond_wait(&pong_done_signal, &pong_done_lock);
	}
	pthread_mutex_unlock(&pong_done_lock);

	pthread_cancel(reader_thread);
	pthread_cancel(event_thread);

	pthread_join(reader_thread, NULL);
	pthread_join(event_thread, NULL);

	firefly_event_queue_free(&event_queue);
	firefly_transport_llp_udp_posix_free(&llp);

	pong_pass_test(TEST_DONE);
	pingpong_test_print_results(pong_tests, NBR_TESTS);
}
