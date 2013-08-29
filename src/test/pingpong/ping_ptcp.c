#include "test/pingpong/ping_ptcp.h"

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <err.h>

#include <labcomm.h>
#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_tcp_posix.h>
#include <utils/firefly_event_queue.h>
#include <utils/firefly_event_queue_posix.h>
#include <gen/pingpong.h>

#include "test/pingpong/pingpong_ptcp.h"
#include "test/pingpong/pingpong.h"
#include "utils/cppmacros.h"

void *event_thread_main(void *args);
void *send_data(void *args);
void ping_handle_pingpong_data(pingpong_data *data, void *ctx);

/* Corresponds to indexes of the arrays below. Note the order. */
enum ping_test_id {
	CONNECTION_OPEN,
	CHAN_OPEN,
	CHAN_RESTRICTED,
	DATA_SEND,
	DATA_RECEIVE,
	CHAN_UNRESTRICTED,
	CHAN_CLOSE,
	TEST_DONE,
	PING_NBR_TESTS
};

static char *ping_test_names[] = {
	"Open connection",
	"Open channel (requesting party)",
	"Restrict channel (requesting party)",
	"Send data",
	"Receive data",
	"Unrestrict channel (requesting party)",
	"Close channel",
	"Ping done"
};

static struct pingpong_test ping_tests[PING_NBR_TESTS];

void ping_init_tests()
{
	for (int i = 0; i < PING_NBR_TESTS; i++)
		pingpong_test_init(&ping_tests[i], ping_test_names[i]);
}

void ping_pass_test(enum ping_test_id test_id)
{
	pingpong_test_pass(&ping_tests[test_id]);
}

/* Global event queue used by all connections. */
static struct firefly_event_queue *event_queue;
static pthread_mutex_t ping_done_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ping_done_signal = PTHREAD_COND_INITIALIZER;
static bool ping_done;

void ping_connection_opened(struct firefly_connection *conn)
{
	printf("PING: conn opened\n");
	ping_pass_test(CONNECTION_OPEN);
	firefly_channel_open(conn);
}

/* Restr. state change. */
void ping_chan_restr_info(struct firefly_channel *chan,
			  enum restriction_transition restr)
{
	UNUSED_VAR(chan);

	switch (restr) {
	case UNRESTRICTED:
		/* Done */
		printf("PING: chan unrestricted\n");
		ping_pass_test(CHAN_UNRESTRICTED);
		firefly_channel_close(chan);
		break;
	case RESTRICTED:
		printf("PING: chan restricted\n");
		ping_pass_test(CHAN_RESTRICTED);
		/* Send data */
		send_data(chan);
		break;
	case RESTRICTION_DENIED:
		/* Fail */
		warnx("ping restriction denied");
		break;
	}
}

void ping_chan_opened(struct firefly_channel *chan)
{
	printf("PING: chan opened\n");
	struct labcomm_encoder *enc;
	struct labcomm_decoder *dec;

	ping_pass_test(CHAN_OPEN);
	enc = firefly_protocol_get_output_stream(chan);
	dec = firefly_protocol_get_input_stream(chan);
	labcomm_decoder_register_pingpong_data(dec, ping_handle_pingpong_data,
					       chan);
	labcomm_encoder_register_pingpong_data(enc);
	firefly_channel_restrict(chan);
}

void ping_chan_closed(struct firefly_channel *chan)
{
	printf("PING: chan closed\n");
	ping_pass_test(CHAN_CLOSE);
	firefly_connection_close(firefly_channel_get_connection(chan));
	pthread_mutex_lock(&ping_done_lock);
	ping_done = true;
	pthread_cond_signal(&ping_done_signal);
	pthread_mutex_unlock(&ping_done_lock);
}

static void ping_channel_error(struct firefly_channel *chan,
		enum firefly_error reason, const char *msg)
{
	printf("PING: chan error\n");
	UNUSED_VAR(chan);
	UNUSED_VAR(msg);
	if (reason == FIREFLY_ERROR_CHAN_REFUSED)
		warnx("Channel was rejected.");
}

void *send_data(void *args)
{
	struct firefly_channel *chan;
	struct labcomm_encoder *enc;
	pingpong_data data = PING_DATA;

	chan = (struct firefly_channel *) args;
	enc = firefly_protocol_get_output_stream(chan);
	labcomm_encode_pingpong_data(enc, &data);
	ping_pass_test(DATA_SEND);

	return NULL;
}

void ping_handle_pingpong_data(pingpong_data *data, void *ctx)
{
	UNUSED_VAR(ctx);
	printf("PING: recieved data\n");
	if (*data == PONG_DATA)
		ping_pass_test(DATA_RECEIVE);
	firefly_channel_unrestrict(ctx);
}

struct firefly_connection_actions ping_actions = {
	.channel_opened		= ping_chan_opened,
	.channel_closed		= ping_chan_closed,
	.channel_recv		= NULL,
	.channel_error		= ping_channel_error,
	// New -v
	.channel_restrict	= NULL,
	.channel_restrict_info	= ping_chan_restr_info,
	.connection_opened = ping_connection_opened
};

void *ping_main_thread(void *arg)
{
	int res;
	struct firefly_transport_llp *llp;

	UNUSED_VAR(arg);
	printf("Hello, Firefly from Ping!\n");
	ping_init_tests();
	event_queue = firefly_event_queue_posix_new(20);
	res = firefly_event_queue_posix_run(event_queue, NULL);
	if (res) fprintf(stderr, "ERROR: starting event thread.");

	llp = firefly_transport_llp_tcp_posix_new(PING_PORT, NULL, event_queue);

	res = firefly_connection_open(&ping_actions, NULL, event_queue,
			firefly_transport_connection_tcp_posix_new(
					llp, -1, PONG_ADDR, PONG_PORT));
	if (res < 0) fprintf(stderr, "PING ERROR: Open connection: %d.\n", res);

	res = firefly_transport_tcp_posix_run(llp);
	if (res) fprintf(stderr, "ERROR: starting reader thread.\n");

	pthread_mutex_lock(&ping_done_lock);
	while (!ping_done)
		pthread_cond_wait(&ping_done_signal, &ping_done_lock);
	pthread_mutex_unlock(&ping_done_lock);
	firefly_transport_tcp_posix_stop(llp);
	firefly_transport_llp_tcp_posix_free(llp);
	firefly_event_queue_posix_free(&event_queue);
	ping_pass_test(TEST_DONE);
	pingpong_test_print_results(ping_tests, PING_NBR_TESTS, "Ping");

	return NULL;
}
