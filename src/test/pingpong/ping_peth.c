#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

#include <labcomm.h>
#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_eth_posix.h>
#include <utils/firefly_event_queue.h>
#include <gen/pingpong.h>

#include "test/pingpong/pingpong_peth.h"
#include "test/pingpong/pingpong.h"
#include "test/pingpong/hack_lctypes.h"
#include "utils/cppmacros.h"

#define PING_NBR_TESTS (6)

void *event_thread_main(void *args);
void *send_data(void *args);

void ping_handle_pingpong_data(pingpong_data *data, void *ctx);

static char *ping_test_names[] = {
	"Open connection",
	"Open channel",
	"Send data",
	"Receive data",
	"Close channel",
	"Ping done"
};

static struct pingpong_test ping_tests[PING_NBR_TESTS];

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
	for (int i = 0; i < PING_NBR_TESTS; i++) {
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

void ping_chan_opened(struct firefly_channel *chan)
{
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	struct labcomm_decoder *dec = firefly_protocol_get_input_stream(chan);

	labcomm_decoder_register_pingpong_data(dec, ping_handle_pingpong_data, chan);
	labcomm_encoder_register_pingpong_data(enc);

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

void ping_chan_closed(struct firefly_channel *chan)
{
	firefly_connection_close(
			firefly_channel_get_connection(chan));
	pthread_mutex_lock(&ping_done_lock);
	ping_done = true;
	pthread_cond_signal(&ping_done_signal);
	pthread_mutex_unlock(&ping_done_lock);
	ping_pass_test(CHAN_CLOSE);
}

bool ping_chan_received(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	printf("PING: Error: Channel received\n");
	return false;
}

void ping_channel_rejected(struct firefly_connection *conn)
{
	UNUSED_VAR(conn);
	fprintf(stderr, "ERROR: Channel was rejected.\n");
}

struct firefly_connection *ping_connection_received(
		struct firefly_transport_llp *llp, char *mac_addr)
{
	printf("PING: Connection received.\n");
	struct firefly_connection *conn = NULL;
	/* If address is correct, open a connection. */
	if (strncmp(mac_addr, PONG_MAC_ADDR, strlen(PONG_MAC_ADDR)) == 0) {
		printf("PING: Connection accepted.\n");
		conn = firefly_transport_connection_eth_posix_open(
				ping_chan_opened, ping_chan_closed, ping_chan_received,
				mac_addr, PING_IFACE, llp);
	}
	return conn;
}

void *send_data(void *args)
{
	struct firefly_channel *chan = (struct firefly_channel *) args;
	pingpong_data data = PING_DATA;
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	labcomm_encode_pingpong_data(enc, &data);
	ping_pass_test(DATA_SEND);
	return NULL;
}

void ping_handle_pingpong_data(pingpong_data *data, void *ctx)
{
	UNUSED_VAR(ctx);
	/*struct firefly_channel *chan = (struct firefly_channel *) ctx;*/
	if (*data == PONG_DATA) {
		ping_pass_test(DATA_RECEIVE);
	}
}

void *ping_main_thread(void *arg)
{
	uid_t uid;
	uid = geteuid();
	if (uid != 0) {
		fprintf(stderr, "Need root to run these tests\n");
		return NULL;
	}
	UNUSED_VAR(arg);
	int res;
	struct event_queue_signals eq_s;
	pthread_t reader_thread;
	pthread_t event_thread;

	printf("Hello, Firefly Ethernet from Ping!\n");
	ping_init_tests();

	res = pthread_mutex_init(&eq_s.eq_lock, NULL);
	if (res) {
		fprintf(stderr, "ERROR: init mutex.\n");
	}
	res = pthread_cond_init(&eq_s.eq_cond, NULL);
	if (res) {
		fprintf(stderr, "ERROR: init cond variable.\n");
	}
	eq_s.event_exec_finish = false;
	event_queue = firefly_event_queue_new(event_add_mutex, &eq_s);
	res = pthread_create(&event_thread, NULL, event_thread_main, event_queue);
	if (res) {
		fprintf(stderr, "ERROR: starting event thread.\n");
	}

	struct firefly_transport_llp *llp =
			firefly_transport_llp_eth_posix_new(PING_IFACE,
					ping_connection_received, event_queue);

	struct firefly_connection *conn =
			firefly_transport_connection_eth_posix_open(ping_chan_opened, ping_chan_closed,
					ping_chan_received, PONG_MAC_ADDR, PONG_IFACE, llp);
	if (conn != NULL) {
		ping_pass_test(CONNECTION_OPEN);
	}
	hack_register_protocol_types(conn);

	firefly_channel_open(conn, ping_channel_rejected);

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
	pthread_join(reader_thread, NULL);

	firefly_transport_llp_eth_posix_free(llp);
	pthread_mutex_lock(&eq_s.eq_lock);
	eq_s.event_exec_finish = true;
	pthread_cond_signal(&eq_s.eq_cond);
	pthread_mutex_unlock(&eq_s.eq_lock);

	pthread_join(event_thread, NULL);
	firefly_event_queue_free(&event_queue);

	ping_pass_test(TEST_DONE);
	pingpong_test_print_results(ping_tests, PING_NBR_TESTS, "Ping");

	return NULL;
}
