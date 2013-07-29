#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#include <labcomm.h>
#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_udp_posix.h>
#include <transport/firefly_transport_eth_posix.h>
#include <utils/firefly_event_queue.h>
#include <utils/firefly_event_queue_posix.h>
#include <gen/pingpong.h>

#include "test/pingpong/pingpong.h"
#include "utils/cppmacros.h"

#define PING_NBR_TESTS (4)

void *event_thread_main(void *args);
void *send_data(void *args);

void ping_handle_pingpong_data(pingpong_data *data, void *ctx);

enum ping_test_id {
	ETH_SEND,
	ETH_RECV,
	UDP_SEND,
	UDP_RECV
};

enum test_phase {
	PHASE_ETH,
	PHASE_UDP
};

static char *ping_test_names[] = {
	"Ethernet send",
	"Ethernet recv",
	"UDP send",
	"UDP recv"
};

static struct pingpong_test ping_tests[PING_NBR_TESTS];
static enum test_phase current_test_phase;	/* Used in callbacks for passing tests. */

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
static bool eth_ping_done = false;
static bool udp_ping_done = false;

void ping_chan_opened(struct firefly_channel *chan)
{
	struct labcomm_encoder *enc;
	struct labcomm_decoder *dec;
	pthread_t sender;
	pthread_attr_t tattr;

	enc = firefly_protocol_get_output_stream(chan);
	dec = firefly_protocol_get_input_stream(chan);

	labcomm_decoder_register_pingpong_data(dec, ping_handle_pingpong_data, chan);
	labcomm_encoder_register_pingpong_data(enc);

	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	pthread_create(&sender, &tattr, send_data, chan);
}

void ping_chan_closed(struct firefly_channel *chan)
{
	firefly_connection_close(firefly_channel_get_connection(chan));
	pthread_mutex_lock(&ping_done_lock);
	switch (current_test_phase) {
	case PHASE_ETH:	eth_ping_done = true; break;
	case PHASE_UDP:	udp_ping_done = true; break;
	}
	pthread_cond_signal(&ping_done_signal);
	pthread_mutex_unlock(&ping_done_lock);
}

bool ping_chan_received(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	printf("PING: Channel received\n");
	return true;
}

void ping_channel_rejected(struct firefly_connection *conn)
{
	UNUSED_VAR(conn);
	fprintf(stderr, "ERROR: Channel was rejected.\n");
}

void ping_connection_opened(struct firefly_connection *conn)
{
	/*ping_pass_test(CONNECTION_OPEN);*/
	printf("Connection open\n");
	firefly_channel_open(conn);
}

struct firefly_connection_actions ping_eth_conn_actions = {
	.channel_opened		= ping_chan_opened,
	.channel_closed		= ping_chan_closed,
	.channel_recv		= ping_chan_received,
	// New -v
	.channel_rejected	= NULL,
	.channel_restrict	= NULL,
	.channel_restrict_info	= NULL,
	.connection_opened = ping_connection_opened
};

struct firefly_connection_actions ping_udp_conn_actions = {
	.channel_opened		= ping_chan_opened,
	.channel_closed		= ping_chan_closed,
	.channel_recv		= ping_chan_received,
	// New -v
	.channel_rejected	= NULL,
	.channel_restrict	= NULL,
	.channel_restrict_info	= NULL,
	.connection_opened = ping_connection_opened
};

void *send_data(void *args)
{
	struct firefly_channel *chan = (struct firefly_channel *) args;
	pingpong_data data = PING_DATA;
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	labcomm_encode_pingpong_data(enc, &data);
	switch (current_test_phase) {
	case PHASE_ETH: ping_pass_test(ETH_SEND); break;
	case PHASE_UDP: ping_pass_test(UDP_SEND); break;
	}

	return NULL;
}

void ping_handle_pingpong_data(pingpong_data *data, void *ctx)
{
	UNUSED_VAR(ctx);
	/*struct firefly_channel *chan = (struct firefly_channel *) ctx;*/
	if (*data == PONG_DATA) {
		switch (current_test_phase) {
		case PHASE_ETH: ping_pass_test(ETH_RECV); break;
		case PHASE_UDP: ping_pass_test(UDP_RECV); break;
		}
	}
}

/* TODO: Rename the old ones and reuse.  */
static void *udp_reader_thread_main(void *args)
{
	struct firefly_transport_llp *llp;

	llp = (struct firefly_transport_llp *) args;
	while (1)
		firefly_transport_udp_posix_read(llp);

	return NULL;
}

static void *eth_reader_thread_main(void *args)
{
	struct firefly_transport_llp *llp;

	llp = (struct firefly_transport_llp *) args;
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	while (1)
		firefly_transport_eth_posix_read(llp, &tv);

	return NULL;
}

void *ping_main_thread(void *arg)
{
	UNUSED_VAR(arg);
	int res;
	uid_t uid;
	pthread_t reader_thread;
	struct firefly_transport_llp *eth_llp;
	struct firefly_transport_llp *llp;

	uid = geteuid();
	if (uid != 0) {
		fprintf(stderr, "Need root to ping with raw ethernet.\n");
		return NULL;
	}

	event_queue = firefly_event_queue_posix_new(20);
	res = firefly_event_queue_posix_run(event_queue, NULL);
	if (res) {
		fprintf(stderr, "ERROR: starting event thread.\n");
	}
	ping_init_tests();

	/* eth. ping */
	current_test_phase = PHASE_ETH;

	eth_llp = firefly_transport_llp_eth_posix_new(PING_IFACE, NULL, event_queue);
	if (eth_llp)
		printf("LLP open\n");
	else
		fprintf(stderr, "Failed to open llp!\n");


	firefly_connection_open(&ping_eth_conn_actions, NULL, event_queue,
			firefly_transport_connection_eth_posix_new(
			eth_llp, PONG_MAC_ADDR, PING_IFACE));

	pthread_create(&reader_thread, NULL, eth_reader_thread_main, eth_llp);

	pthread_mutex_lock(&ping_done_lock);
	while (!eth_ping_done)
		pthread_cond_wait(&ping_done_signal, &ping_done_lock);
	pthread_mutex_unlock(&ping_done_lock);

	pthread_cancel(reader_thread);
	pthread_join(reader_thread, NULL);
	/* printf("Freeing %p\n", &eth_llp); */
	firefly_transport_llp_eth_posix_free(eth_llp);
	printf("Ethernet phase done!\n");

	/* udp ping */
	current_test_phase = PHASE_UDP;

	llp = firefly_transport_llp_udp_posix_new(PING_PORT, NULL, event_queue);

	firefly_connection_open(&ping_udp_conn_actions, NULL, event_queue,
			firefly_transport_connection_udp_posix_new(
					llp, PONG_ADDR, PONG_PORT,
					FIREFLY_TRANSPORT_UDP_POSIX_DEFAULT_TIMEOUT));

	pthread_create(&reader_thread, NULL, udp_reader_thread_main, llp);

	pthread_mutex_lock(&ping_done_lock);
	while (!udp_ping_done)
		pthread_cond_wait(&ping_done_signal, &ping_done_lock);
	pthread_mutex_unlock(&ping_done_lock);

	pthread_cancel(reader_thread);

	pthread_join(reader_thread, NULL);

	firefly_transport_llp_udp_posix_free(llp);
	firefly_event_queue_posix_free(&event_queue);

	pingpong_test_print_results(ping_tests, PING_NBR_TESTS, "Ping");

	return NULL;
}
