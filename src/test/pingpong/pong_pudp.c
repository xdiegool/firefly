#include "test/pingpong/pong_pudp.h"

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <err.h>

#include <labcomm.h>
#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_udp_posix.h>
#include <utils/firefly_event_queue.h>
#include <utils/firefly_event_queue_posix.h>
#include <gen/pingpong.h>

#include "test/pingpong/pingpong_pudp.h"
#include "test/pingpong/pingpong.h"
#include "utils/cppmacros.h"


static char *pong_test_names[] = {
	"Open connection",
	"Received channel",
	"Opened channel (responding party)",
	"Restricted channel (responding party)",
	"Receive data",
	"Send data",
	"Unrestrict channel (responding party)",
	"Close channel",
	"Pong done"
};

static struct pingpong_test pong_tests[PONG_NBR_TESTS];

/* Corresponds to indexes to the above arrays. Note the order. */
enum pong_test_id {
	CONNECTION_OPEN,
	CHAN_RECEIVE,
	CHAN_OPENED,
	CHAN_RESTRICTED,
	DATA_RECEIVE,
	DATA_SEND,
	CHAN_UNRESTRICTED,
	CHAN_CLOSE,
	TEST_DONE
};

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

void pong_chan_opened(struct firefly_channel *chan);
void pong_chan_closed(struct firefly_channel *chan);
bool pong_chan_received(struct firefly_channel *chan);
void pong_handle_pingpong_data(pingpong_data *data, void *ctx);
void *send_data_and_close(void *args);

/* Restr. state change. */
void pong_chan_restr_info(struct firefly_channel *chan,
			  enum restriction_transition restr)
{
	UNUSED_VAR(chan);
	switch (restr) {
	case UNRESTRICTED:
		pong_pass_test(CHAN_UNRESTRICTED);
		break;
	case RESTRICTED: {
		warnx("pong restricted. Should no happen "
		     "this way, pong is passive!");
		break;
	}
	case RESTRICTION_DENIED:
		warnx("Pong restriction denied. Pong should be passive!");
		break;
	}
}

/* Incoming restr. req. */
bool pong_chan_restr(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);

	pong_pass_test(CHAN_RESTRICTED);

	return true;
}

struct firefly_connection *pong_connection_received(
		struct firefly_transport_llp *llp, const char *ip_addr, unsigned short port)
{
	struct firefly_connection *conn = NULL;
	/* If address is correct, open a connection. */
	if (strncmp(ip_addr, PING_ADDR, strlen(PING_ADDR)) == 0 &&
			port == PING_PORT) {
		conn = firefly_transport_connection_udp_posix_open(
				pong_chan_opened, pong_chan_closed, pong_chan_received,
				ip_addr, port, FIREFLY_TRANSPORT_UDP_POSIX_DEFAULT_TIMEOUT, llp);
		pong_pass_test(CONNECTION_OPEN);
		firefly_connection_enable_restricted_channels(conn,
				pong_chan_restr_info, pong_chan_restr);

	} else {
		fprintf(stderr, "ERROR: Received unknown connection: %s:%hu\n",
				ip_addr, port);
	}
	return conn;
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

	/* The protocol events takes precedence over the ones spawned above.
	 * If we want to test in this fashion we will have to simulate
	 * using the channel for arbitrary length of time. This will
	 * give the system time to send the application data before closing
	 * the channel. For this kind of short bursts a synchronous mode *might*
	 * be a feature in the future. This, however, is not likely. Should the
	 * application need to keep track of state above the protocol level,
	 * *it* should probably do so.
	 */
	sleep(1);
	firefly_channel_close(chan);

	return NULL;
}

void *pong_main_thread(void *arg)
{
	int res;
	pthread_t reader_thread;
	pthread_t resend_thread;
	struct thread_arg *ta = (struct thread_arg *) arg;

	printf("Hello, Firefly from Pong!\n");
	pong_init_tests();

	event_queue = firefly_event_queue_posix_new(20);
	res = firefly_event_queue_posix_run(event_queue, NULL);
	if (res) {
		fprintf(stderr, "ERROR: starting event thread.\n");
	}

	struct firefly_transport_llp *llp =
			firefly_transport_llp_udp_posix_new(PONG_PORT,
					pong_connection_received, event_queue);

	res = firefly_transport_udp_posix_run(llp, &reader_thread, &resend_thread);
	/*res = pthread_create(&reader_thread, NULL, reader_thread_main, llp);*/
	if (res) {
		fprintf(stderr, "ERROR: starting reader thread.\n");
	}

	// Signal to pingpong_main that pong is started so ping can start now.
	pthread_mutex_lock(&ta->m);
	ta->started = true;
	pthread_cond_signal(&ta->t);
	pthread_mutex_unlock(&ta->m);

	pthread_mutex_lock(&pong_done_lock);
	while (!pong_done) {
		pthread_cond_wait(&pong_done_signal, &pong_done_lock);
	}
	pthread_mutex_unlock(&pong_done_lock);

	firefly_transport_udp_posix_stop(llp, &reader_thread, &resend_thread);

	firefly_transport_llp_udp_posix_free(llp);

	firefly_event_queue_posix_free(&event_queue);

	pong_pass_test(TEST_DONE);
	pingpong_test_print_results(pong_tests, PONG_NBR_TESTS, "Pong");

	return NULL;
}
