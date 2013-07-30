#include "test/pingpong/pong_pudp.h"

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <execinfo.h>
#include <getopt.h>
#include <signal.h>

#include <labcomm.h>
#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_eth_posix.h>
#include <utils/firefly_event_queue.h>
#include <utils/firefly_event_queue_posix.h>
#include <gen/pingpong.h>

#include "test/pingpong/pingpong_peth.h"
#include "test/pingpong/pingpong.h"
#include "utils/cppmacros.h"

enum pong_test_id {
	CONNECTION_OPEN,
	CHAN_RECEIVE,
	CHAN_OPENED,
	DATA_SEND,
	DATA_RECEIVE,
	CHAN_CLOSE,
	TEST_DONE,
	PONG_NBR_TESTS
};

static char *pong_test_names[] = {
	"Open connection",
	"Received channel",
	"Opened channel",
	"Send data",
	"Receive data",
	"Close channel",
	"Ping done"
};

static struct pingpong_test pong_tests[PONG_NBR_TESTS];

void pong_init_tests()
{
	for (int i = 0; i < PONG_NBR_TESTS; i++) {
		pingpong_test_init(&pong_tests[i], pong_test_names[i]);
	}
}

void pong_pass_test(enum pong_test_id test_id)
{
	pingpong_test_pass(&pong_tests[test_id]);
}

static struct firefly_event_queue *event_queue;
static pthread_mutex_t pong_done_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pong_done_signal = PTHREAD_COND_INITIALIZER;
static bool pong_done = false;
static char *ping_mac_addr = PING_MAC_ADDR;
static char *pong_iface = PING_IFACE;

void pong_chan_opened(struct firefly_channel *chan);
void pong_chan_closed(struct firefly_channel *chan);
bool pong_chan_received(struct firefly_channel *chan);
void pong_handle_pingpong_data(pingpong_data *data, void *ctx);
void *send_data_and_close(void *args);
bool pong_chan_on_restrict_request(struct firefly_channel *chan);
void pong_chan_on_restrict_change(struct firefly_channel *chan,
		enum restriction_transition rinfo);
void ping_connection_opened(struct firefly_connection *conn);

struct firefly_connection_actions conn_actions = {
	.channel_opened		= pong_chan_opened,
	.channel_closed		= pong_chan_closed,
	.channel_recv		= pong_chan_received,
	// New -v
	.channel_rejected	= NULL,
	.channel_restrict	= pong_chan_on_restrict_request,
	.channel_restrict_info	= pong_chan_on_restrict_change,
	.connection_opened = ping_connection_opened
};

void pong_chan_on_restrict_change(struct firefly_channel *chan,
		enum restriction_transition rinfo)
{
	UNUSED_VAR(chan);
	UNUSED_VAR(rinfo);

	printf("restrict change\n");
}

bool pong_chan_on_restrict_request(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	return true;
}

void ping_connection_opened(struct firefly_connection *conn)
{
	UNUSED_VAR(conn);
	pong_pass_test(CONNECTION_OPEN);
}

int64_t pong_connection_received(struct firefly_transport_llp *llp,
		char *mac_addr)
{
	/* If address is correct, open a connection. */
	if (strncmp(mac_addr, ping_mac_addr, strlen(ping_mac_addr)) == 0) {
		return firefly_connection_open(&conn_actions, NULL, event_queue,
				firefly_transport_connection_eth_posix_new(
				llp, mac_addr, pong_iface));
	} else {
		fprintf(stderr, "ERROR: Received unknown connection: %s\n",
				mac_addr);
	}
	return 0;
}

void pong_chan_opened(struct firefly_channel *chan)
{
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	struct labcomm_decoder *dec = firefly_protocol_get_input_stream(chan);

	labcomm_decoder_register_pingpong_data(dec, pong_handle_pingpong_data, chan);
	labcomm_encoder_register_pingpong_data(enc);
	pong_pass_test(CHAN_OPENED);
}

void pong_chan_closed(struct firefly_channel *chan)
{
	firefly_connection_close(
			firefly_channel_get_connection(chan));
	pthread_mutex_lock(&pong_done_lock);
	pong_done = true;
	pthread_cond_signal(&pong_done_signal);
	pong_pass_test(CHAN_CLOSE);
	pthread_mutex_unlock(&pong_done_lock);
}

bool pong_chan_received(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	pong_pass_test(CHAN_RECEIVE);
	return true;
}

void pong_channel_rejected(struct firefly_connection *conn)
{
	UNUSED_VAR(conn);
	fprintf(stderr, "ERROR: Channel rejected.\n");
}

void pong_handle_pingpong_data(pingpong_data *data, void *ctx)
{
	UNUSED_VAR(data);
	UNUSED_VAR(ctx);
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
	pingpong_data data = PONG_DATA;
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	labcomm_encode_pingpong_data(enc, &data);
	pong_pass_test(DATA_SEND);

	return NULL;
}


int main(int argc, char** argv)
{
	uid_t uid;
	uid = geteuid();
	if (uid != 0) {
		fprintf(stderr, "Need root to run these tests\n");
		return 1;
	}
	int res;
	struct reader_thread_args rtarg;
	pthread_attr_t thread_attrs;
	pthread_t reader_thread;

	printf("Hello, Firefly Ethernet from Pong!\n");
	pong_init_tests();

	pong_iface = PONG_IFACE;
	ping_mac_addr = PING_MAC_ADDR;

	for (int i = 1; i < argc; i += 2) {
		if (strncmp(argv[i], "-i", 2) == 0) {
			pong_iface = argv[i+1];
		} else if (strncmp(argv[i], "-m", 2) == 0) {
			ping_mac_addr = argv[i+1];
		}
	}

	event_queue = firefly_event_queue_posix_new(20);
	pthread_attr_init(&thread_attrs);
	res = firefly_event_queue_posix_run(event_queue, &thread_attrs);
	if (res) {
		fprintf(stderr, "ERROR: starting event thread.\n");
	}

	struct firefly_transport_llp *llp =
			firefly_transport_llp_eth_posix_new(pong_iface,
					pong_connection_received, event_queue);

	res = pthread_mutex_init(&rtarg.lock, NULL);
	if (res) {
		fprintf(stderr, "ERROR: init mutex.\n");
	}
	rtarg.finish = false;
	rtarg.llp = llp;

	res = pthread_create(&reader_thread, &thread_attrs, reader_thread_main,
			&rtarg);
	if (res) {
		fprintf(stderr, "ERROR: starting reader thread.\n");
	}
	pthread_attr_destroy(&thread_attrs);

	// Pong is started

	pthread_mutex_lock(&pong_done_lock);
	while (!pong_done) {
		pthread_cond_wait(&pong_done_signal, &pong_done_lock);
	}
	pthread_mutex_unlock(&pong_done_lock);

	pthread_mutex_lock(&rtarg.lock);
	rtarg.finish = true;
	pthread_mutex_unlock(&rtarg.lock);
	pthread_join(reader_thread, NULL);

	firefly_transport_llp_eth_posix_free(llp);
	firefly_event_queue_posix_free(&event_queue);

	pong_pass_test(TEST_DONE);
	pingpong_test_print_results(pong_tests, PONG_NBR_TESTS, "Pong");

	return 0;
}
