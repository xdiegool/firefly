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
#include "test/pingpong/pong_pmulti.h"

#define PONG_NBR_TESTS (4)

enum pong_test_id {
	ETH_RECV,
	ETH_SEND,
	UDP_RECV,
	UDP_SEND
};

enum test_phase {
	PHASE_ETH,
	PHASE_UDP
};

static char *pong_test_names[] = {
	"Ethernet recv",
	"Ethernet send",
	"UDP recv",
	"UDP send"
};

static struct pingpong_test pong_tests[PONG_NBR_TESTS];
static enum test_phase current_test_phase;	/* Used in callbacks for passing tests. */

void pong_init_tests()
{
	for (int i = 0; i < PONG_NBR_TESTS; i++)
		pingpong_test_init(&pong_tests[i], pong_test_names[i]);
}

void pong_pass_test(enum pong_test_id test_id)
{
	pingpong_test_pass(&pong_tests[test_id]);
}

static struct firefly_event_queue *event_queue;
static pthread_mutex_t pong_done_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pong_done_signal = PTHREAD_COND_INITIALIZER;
static bool eth_pong_done = false;
static bool udp_pong_done = false;

void pong_chan_opened(struct firefly_channel *chan);
void pong_udp_chan_closed(struct firefly_channel *chan);
bool pong_chan_received(struct firefly_channel *chan);
void pong_handle_pingpong_data(pingpong_data *data, void *ctx);
void pong_pudp_connection_opened(struct firefly_connection *conn);
void *send_data_and_close(void *args);

struct firefly_connection_actions pong_udp_conn_actions = {
	.channel_opened		= pong_chan_opened,
	.channel_closed		= pong_udp_chan_closed,
	.channel_recv		= pong_chan_received,
	// New -v
	.channel_rejected	= NULL,
	.channel_restrict	= NULL,
	.channel_restrict_info	= NULL,
	.connection_opened = pong_pudp_connection_opened
};

void pong_pudp_connection_opened(struct firefly_connection *conn)
{
	UNUSED_VAR(conn);
	/*pong_pass_test(CONNECTION_OPEN);*/
}

bool pong_udp_connection_received(
							struct firefly_transport_llp *llp,
							const char *ip_addr,
							unsigned short port)
{

	if (strncmp(ip_addr, PING_ADDR, strlen(PING_ADDR)) == 0 &&
		port == PING_PORT)
	{
		firefly_connection_open(&pong_udp_conn_actions, NULL, event_queue,
				firefly_transport_connection_udp_posix_new(
						llp, ip_addr, port,
						FIREFLY_TRANSPORT_UDP_POSIX_DEFAULT_TIMEOUT));
		return true;
	} else {
		fprintf(stderr, "ERROR: Received unknown connection: %s:%hu\n",
				ip_addr, port);
		return false;
	}
}

void pong_chan_opened(struct firefly_channel *chan)
{
	struct labcomm_encoder *enc;
	struct labcomm_decoder *dec;

	enc = firefly_protocol_get_output_stream(chan);
	dec = firefly_protocol_get_input_stream(chan);

	labcomm_decoder_register_pingpong_data(dec, pong_handle_pingpong_data, chan);
	labcomm_encoder_register_pingpong_data(enc);
}

void pong_udp_chan_closed(struct firefly_channel *chan)
{
	printf("Closing UDP channel...\n");
	firefly_connection_close(firefly_channel_get_connection(chan));

	pthread_mutex_lock(&pong_done_lock);
	udp_pong_done = true;
	pthread_cond_signal(&pong_done_signal);
	pthread_mutex_unlock(&pong_done_lock);
	printf(" Done!\n");
}

bool pong_chan_received(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
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
	int err;

	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	err = pthread_create(&sender, &tattr, send_data_and_close, ctx);
	if (err) {
		fprintf(stderr, "channel_received: Could not create sender thread"
				" for channel.\n");
	}
	switch (current_test_phase) {
	case PHASE_ETH: pong_pass_test(ETH_RECV); break;
	case PHASE_UDP: pong_pass_test(UDP_RECV); break;
	}
}

void *send_data_and_close(void *args)
{
	struct firefly_channel *chan = (struct firefly_channel *) args;
	pingpong_data data = PONG_DATA;
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	labcomm_encode_pingpong_data(enc, &data);

	switch (current_test_phase) {
	case PHASE_ETH: pong_pass_test(ETH_SEND); break;
	case PHASE_UDP: pong_pass_test(UDP_SEND); break;
	}

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

void pong_eth_chan_closed(struct firefly_channel *chan)
{
	firefly_connection_close(firefly_channel_get_connection(chan));
	pthread_mutex_lock(&pong_done_lock);
	eth_pong_done = true;
	pthread_cond_signal(&pong_done_signal);
	pthread_mutex_unlock(&pong_done_lock);
}

void pong_eth_connection_opened(struct firefly_connection *conn)
{
	UNUSED_VAR(conn);
	/*pong_pass_test(CONNECTION_OPEN);*/
}

struct firefly_connection_actions pong_eth_conn_actions = {
	.channel_opened		= pong_chan_opened,
	.channel_closed		= pong_eth_chan_closed,
	.channel_recv		= pong_chan_received,
	// New -v
	.channel_rejected	= NULL,
	.channel_restrict	= NULL,
	.channel_restrict_info	= NULL,
	.connection_opened = pong_eth_connection_opened
};

bool pong_eth_connection_received(struct firefly_transport_llp *llp,
							char *mac_addr)
{
	/* If address is correct, open a connection. */
	if (strncmp(mac_addr, PING_MAC_ADDR, strlen(PING_MAC_ADDR)) == 0) {
		printf("Recieved connection on %s!\n", PONG_IFACE);
		firefly_connection_open(&pong_eth_conn_actions, NULL, event_queue,
				firefly_transport_connection_eth_posix_new(
				llp, mac_addr, PONG_IFACE));
		return true;
	} else {
		fprintf(stderr, "ERROR: Received unknown connection: %s\n", mac_addr);
		return false;
	}
}

static void *udp_reader_thread_main(void *args)
{
	struct firefly_transport_llp *llp;

	llp = (struct firefly_transport_llp *) args;
	while (1)
		firefly_transport_udp_posix_read(llp);
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
}

void *pong_main_thread(void *arg)
{
	UNUSED_VAR(arg);
	int res;
	uid_t uid;
	pthread_t reader_thread;
	struct thread_arg *ta;
	struct firefly_transport_llp *eth_llp;
	struct firefly_transport_llp *llp;

	ta = (struct thread_arg *) arg;
	uid = geteuid();
	if (uid != 0) {
		fprintf(stderr, "Need root to pong with raw ethernet.\n");
		pthread_mutex_lock(&ta->m);
		ta->started = true;
		pthread_cond_signal(&ta->t);
		pthread_mutex_unlock(&ta->m);
		return NULL;
	}

	printf("Multipong started.\n");
	pong_init_tests();
	event_queue = firefly_event_queue_posix_new(20);
	res = firefly_event_queue_posix_run(event_queue, NULL);
	if (res) {
		fprintf(stderr, "ERROR: starting event thread.\n");
	}

	/* Signal to pingpong_main that pong is started so ping can start now.
	 * (Not testable w/o two eth if:s)
	 */
	pthread_mutex_lock(&ta->m);
	ta->started = true;
	pthread_cond_signal(&ta->t);
	pthread_mutex_unlock(&ta->m);

	/* eth. pong */
	current_test_phase = PHASE_ETH;
	eth_llp = firefly_transport_llp_eth_posix_new(PONG_IFACE,
						      pong_eth_connection_received, event_queue);

	pthread_create(&reader_thread, NULL, eth_reader_thread_main, eth_llp);
	printf("Listening on %s...\n", PONG_IFACE);
	pthread_mutex_lock(&pong_done_lock);
	while (!eth_pong_done)
		pthread_cond_wait(&pong_done_signal, &pong_done_lock);
	pthread_mutex_unlock(&pong_done_lock);
	printf("Ethernet phase done!\n");

	/* udp pong */
	current_test_phase = PHASE_UDP;
	llp = firefly_transport_llp_udp_posix_new(PONG_PORT,
						  pong_udp_connection_received, event_queue);
	pthread_create(&reader_thread, NULL, udp_reader_thread_main, llp);

	pthread_mutex_lock(&pong_done_lock);
	while (!udp_pong_done)
		pthread_cond_wait(&pong_done_signal, &pong_done_lock);
	pthread_mutex_unlock(&pong_done_lock);

	printf("UDP phase done!\n");

	pthread_cancel(reader_thread);
	pthread_join(reader_thread, NULL);

	firefly_transport_llp_udp_posix_free(llp);
	firefly_event_queue_posix_free(&event_queue);

	pingpong_test_print_results(pong_tests, PONG_NBR_TESTS, "Pong");

	return NULL;
}
