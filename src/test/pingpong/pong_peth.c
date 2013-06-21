#include "test/pingpong/pong_pudp.h"

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#ifdef __XENO__
#include <native/task.h>
#endif
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

void pong_chan_opened(struct firefly_channel *chan);
void pong_chan_closed(struct firefly_channel *chan);
bool pong_chan_received(struct firefly_channel *chan);
void pong_handle_pingpong_data(pingpong_data *data, void *ctx);
void *send_data_and_close(void *args);

struct firefly_connection_actions conn_actions = {
	.channel_opened		= pong_chan_opened,
	.channel_closed		= pong_chan_closed,
	.channel_recv		= pong_chan_received,
	// New -v
	.channel_rejected	= NULL,
	.channel_restrict	= NULL,
	.channel_restrict_info	= NULL
};


struct firefly_connection *pong_connection_received(
		struct firefly_transport_llp *llp, char *mac_addr)
{
	struct firefly_connection *conn = NULL;
	/* If address is correct, open a connection. */
	if (strncmp(mac_addr, PING_MAC_ADDR, strlen(PING_MAC_ADDR)) == 0) {
		conn = firefly_transport_connection_eth_posix_open(
				llp, mac_addr, PING_IFACE, &conn_actions);
		pong_pass_test(CONNECTION_OPEN);
	} else {
		fprintf(stderr, "ERROR: Received unknown connection: %s\n",
				mac_addr);
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

	return NULL;
}

#ifdef __XENO__
static const char *reason_str[] = {
	[SIGDEBUG_UNDEFINED] = "undefined",
	[SIGDEBUG_MIGRATE_SIGNAL] = "received signal",
	[SIGDEBUG_MIGRATE_SYSCALL] = "invoked syscall",
	[SIGDEBUG_MIGRATE_FAULT] = "triggered fault",
	[SIGDEBUG_MIGRATE_PRIOINV] = "affected by priority inversion",
	[SIGDEBUG_NOMLOCK] = "missing mlockall",
	[SIGDEBUG_WATCHDOG] = "runaway thread",
};

void warn_upon_switch(int sig, siginfo_t *si, void *context)
{
	UNUSED_VAR(sig);
	UNUSED_VAR(context);
    unsigned int reason = si->si_value.sival_int;
    void *bt[32];
    int nentries;

    printf("\nSIGDEBUG received, reason %d: %s\n", reason,
       reason <= SIGDEBUG_WATCHDOG ? reason_str[reason] : "<unknown>");
    /* Dump a backtrace of the frame which caused the switch to
       secondary mode: */
    nentries = backtrace(bt,sizeof(bt) / sizeof(bt[0]));
    backtrace_symbols_fd(bt,nentries,fileno(stdout));
}
#endif

int main()
{
	uid_t uid;
	uid = geteuid();
	if (uid != 0) {
		fprintf(stderr, "Need root to run these tests\n");
		return 1;
	}
	int res;
	struct reader_thread_args rtarg;
	pthread_t reader_thread;
#ifdef __XENO__
	mlockall(MCL_CURRENT | MCL_FUTURE);

	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = warn_upon_switch;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGDEBUG, &sa, NULL);
	/*sigaction(SIGXCPU, &sa, NULL);*/
#endif

	printf("Hello, Firefly Ethernet from Pong!\n");
	pong_init_tests();

	event_queue = firefly_event_queue_posix_new(20);
	pthread_attr_t thread_attrs;
	pthread_attr_init(&thread_attrs);
#ifdef __XENO__
	pthread_attr_setschedpolicy(&thread_attrs, SCHED_FIFO);
	struct sched_param thread_prio = {.sched_priority = 51};
	pthread_attr_setschedparam(&thread_attrs, &thread_prio);
#endif
	res = firefly_event_queue_posix_run(event_queue, &thread_attrs);
	if (res) {
		fprintf(stderr, "ERROR: starting event thread.\n");
	}

	struct firefly_transport_llp *llp =
			firefly_transport_llp_eth_posix_new(PONG_IFACE,
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
