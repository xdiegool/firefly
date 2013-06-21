#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#ifdef __XENO__
#include <native/task.h>
#endif
#include <execinfo.h>
#include <getopt.h>
#include <signal.h>

#include <string.h>
#include <stdio.h>

#include <labcomm.h>
#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_eth_posix.h>
#include <utils/firefly_event_queue.h>
#include <utils/firefly_event_queue_posix.h>
#include <gen/pingpong.h>

#include "test/pingpong/pingpong_peth.h"
#include "test/pingpong/pingpong.h"
#include "utils/cppmacros.h"

void *event_thread_main(void *args);
void *send_data(void *args);

void ping_handle_pingpong_data(pingpong_data *data, void *ctx);

enum ping_test_id {
	CONNECTION_OPEN,
	CHAN_OPEN,
	DATA_SEND,
	DATA_RECEIVE,
	CHAN_CLOSE,
	TEST_DONE,
	PING_NBR_TESTS
};

static char *ping_test_names[] = {
	"Open connection",
	"Open channel",
	"Send data",
	"Receive data",
	"Close channel",
	"Ping done"
};

static struct pingpong_test ping_tests[PING_NBR_TESTS];

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

struct firefly_connection_actions conn_actions = {
	.channel_opened		= ping_chan_opened,
	.channel_closed		= ping_chan_closed,
	.channel_recv		= ping_chan_received,
	// New -v
	.channel_rejected	= ping_channel_rejected,
	.channel_restrict	= NULL,
	.channel_restrict_info	= NULL
};

struct firefly_connection *ping_connection_received(
		struct firefly_transport_llp *llp, char *mac_addr)
{
	printf("PING: Connection received.\n");
	struct firefly_connection *conn = NULL;
	/* If address is correct, open a connection. */
	if (strncmp(mac_addr, PONG_MAC_ADDR, strlen(PONG_MAC_ADDR)) == 0) {
		printf("PING: Connection accepted.\n");
		conn = firefly_transport_connection_eth_posix_open(
				llp, mac_addr, PING_IFACE, &conn_actions);
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
	struct firefly_channel *chan = (struct firefly_channel *) ctx;
	if (*data == PONG_DATA) {
		ping_pass_test(DATA_RECEIVE);
	}
	firefly_channel_close(chan);
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

int main(int argc, char **argv)
{
	uid_t uid;
	uid = geteuid();
	if (uid != 0) {
		fprintf(stderr, "Need root to run these tests\n");
		return 1;
	}
	char *iface;
	char *mac_addr;
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

	printf("Hello, Firefly Ethernet from Ping!\n");
	ping_init_tests();

	iface = PING_IFACE;
	mac_addr = PONG_MAC_ADDR;

	for (int i = 1; i < argc; i += 2) {
		if (strncmp(argv[i], "-i", 2) == 0) {
			iface = argv[i+1];
		} else if (strncmp(argv[i], "-m", 2) == 0) {
			mac_addr = argv[i+1];
		}
	}

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
			firefly_transport_llp_eth_posix_new(iface,
					ping_connection_received, event_queue);

	struct firefly_connection *conn =
		firefly_transport_connection_eth_posix_open(llp,
				mac_addr, iface, &conn_actions);
	if (conn != NULL) {
		ping_pass_test(CONNECTION_OPEN);
	}

	firefly_channel_open(conn);

	res = pthread_mutex_init(&rtarg.lock, NULL);
	if (res) {
		fprintf(stderr, "ERROR: init mutex.\n");
	}
	rtarg.finish = false;
	rtarg.llp = llp;

	res = pthread_create(&reader_thread, &thread_attrs, reader_thread_main, &rtarg);
	if (res) {
		fprintf(stderr, "ERROR: starting reader thread.\n");
	}
	pthread_attr_destroy(&thread_attrs);

	pthread_mutex_lock(&ping_done_lock);
	while (!ping_done) {
		pthread_cond_wait(&ping_done_signal, &ping_done_lock);
	}
	pthread_mutex_unlock(&ping_done_lock);

	pthread_mutex_lock(&rtarg.lock);
	rtarg.finish = true;
	pthread_mutex_unlock(&rtarg.lock);
	pthread_join(reader_thread, NULL);

	firefly_transport_llp_eth_posix_free(llp);

	firefly_event_queue_posix_free(&event_queue);

	ping_pass_test(TEST_DONE);
	pingpong_test_print_results(ping_tests, PING_NBR_TESTS, "Ping");

	return 0;
}
