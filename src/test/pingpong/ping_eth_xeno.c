// STD
#include <string.h>
#include <stdio.h>
#include <unistd.h>

// System
#include <sys/types.h>
#include <sys/mman.h>
#include <execinfo.h>
#include <getopt.h>
#include <signal.h>

// xenomai
#include <native/task.h>
#include <native/mutex.h>
#include <native/sem.h>

#include <labcomm.h>
#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_eth_xeno.h>
#include <utils/firefly_event_queue.h>
#include <utils/firefly_event_queue_xeno.h>
#include <gen/pingpong.h>

#include "test/pingpong/pingpong_eth_xeno.h"
#include "test/pingpong/pingpong.h"
#include "utils/cppmacros.h"

#define EVENT_POOL_SIZE		(20)
#define MAIN_TASK_PRIO		(45)
#define EVENT_LOOP_PRIO		(55)
#define READER_TASK_PRIO	(50)
#define EVENT_TIMEOUT		(1000000) // 1 ms

static RT_TASK *reader_thread;
static RT_TASK *main_thread;
static RT_TASK *event_thread;

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
static RT_SEM ping_done_signal;

void ping_chan_opened(struct firefly_channel *chan)
{
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	struct labcomm_decoder *dec = firefly_protocol_get_input_stream(chan);

	labcomm_decoder_register_pingpong_data(dec, ping_handle_pingpong_data, chan);
	labcomm_encoder_register_pingpong_data(enc);

	firefly_channel_restrict(chan);

	ping_pass_test(CHAN_OPEN);
}

void ping_chan_closed(struct firefly_channel *chan)
{
	firefly_connection_close(
			firefly_channel_get_connection(chan));
	ping_pass_test(CHAN_CLOSE);
	rt_sem_v(&ping_done_signal);
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

void ping_channel_restrict_change(struct firefly_channel *chan,
						 enum restriction_transition rinfo)
{
	if (rinfo == RESTRICTED) {
		int res = rt_task_set_mode(0, T_WARNSW, NULL);
		if (res != 0) {
			fprintf(stderr, "Error set mode %d\n", res);
			return;
		}
		pingpong_data data = PING_DATA;
		struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
		labcomm_encode_pingpong_data(enc, &data);
		ping_pass_test(DATA_SEND);
	}
}

void ping_connection_opened(struct firefly_connection *conn)
{
	ping_pass_test(CONNECTION_OPEN);
	firefly_channel_open(conn);
}

struct firefly_connection_actions conn_actions = {
	.channel_opened		= ping_chan_opened,
	.channel_closed		= ping_chan_closed,
	.channel_recv		= ping_chan_received,
	// New -v
	.channel_rejected	= ping_channel_rejected,
	.channel_restrict	= NULL,
	.channel_restrict_info	= ping_channel_restrict_change,
	.connection_opened = ping_connection_opened
};


void ping_handle_pingpong_data(pingpong_data *data, void *ctx)
{
	struct firefly_channel *chan = (struct firefly_channel *) ctx;
	if (*data == PONG_DATA) {
		ping_pass_test(DATA_RECEIVE);
	}
	firefly_channel_close(chan);
}

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
	RT_TASK *self = rt_task_self();
	if (rt_task_same(self, reader_thread)) printf("Reader thread\n");
	if (rt_task_same(self, main_thread)) printf("Main thread\n");
	if (rt_task_same(self, event_thread)) printf("Event thread\n");

    printf("\nSIGDEBUG received, reason %d: %s\n", reason,
       reason <= SIGDEBUG_WATCHDOG ? reason_str[reason] : "<unknown>");
    /* Dump a backtrace of the frame which caused the switch to
       secondary mode: */
    nentries = backtrace(bt,sizeof(bt) / sizeof(bt[0]));
    backtrace_symbols_fd(bt,nentries,fileno(stdout));
}

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
	RT_TASK reader_task;
	RT_TASK main_task;
	struct reader_thread_args rtarg;

	iface = PING_IFACE;
	mac_addr = PONG_MAC_ADDR;

	for (int i = 1; i < argc; i += 2) {
		if (strncmp(argv[i], "-i", 2) == 0) {
			iface = argv[i+1];
		} else if (strncmp(argv[i], "-m", 2) == 0) {
			mac_addr = argv[i+1];
		}
	}

	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = warn_upon_switch;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGDEBUG, &sa, NULL);

	printf("Hello, Firefly Ethernet from Ping!\n");

	mlockall(MCL_CURRENT | MCL_FUTURE);
	res = rt_task_shadow(&main_task, "main_task", MAIN_TASK_PRIO, 0);
	main_thread = &main_task;
	if (res) {
		fprintf(stderr, "ERROR: Failed shadow task, %s\n", strerror(-res));
	}

	rt_sem_create(&ping_done_signal, "ping_done_sem", 0, S_PRIO);

	ping_init_tests();

	event_queue = firefly_event_queue_xeno_new(EVENT_POOL_SIZE,
			EVENT_LOOP_PRIO, EVENT_TIMEOUT);
	res = firefly_event_queue_xeno_run(event_queue);
	if (res) {
		fprintf(stderr, "ERROR: starting event thread.\n");
	}
	event_thread = firefly_event_queue_xeno_loop(event_queue);

	struct firefly_transport_llp *llp =
			firefly_transport_llp_eth_xeno_new(iface,
					NULL, event_queue);

	firefly_connection_open(&conn_actions,
			firefly_transport_eth_xeno_memfuncs(), event_queue,
			firefly_transport_connection_eth_xeno_new(
			llp, mac_addr, iface));

	res = rt_mutex_create(&rtarg.lock, "reader_mutex");
	if (res) {
		fprintf(stderr, "ERROR: init reader mutex.\n");
	}
	rtarg.finish = false;
	rtarg.llp = llp;

	res = rt_task_create(&reader_task, "reader_task", 0,
			READER_TASK_PRIO, T_FPU|T_JOINABLE);
	reader_thread = &reader_task;
	if (res) {
		fprintf(stderr, "ERROR: creating reader thread.\n");
		return 1;
	}
	rt_task_start(reader_thread, reader_thread_main, &rtarg);

	rt_sem_p(&ping_done_signal, TM_INFINITE);

	rt_mutex_acquire(&rtarg.lock, 1000000);
	rtarg.finish = true;
	rt_mutex_release(&rtarg.lock);
	rt_task_join(reader_thread);

	firefly_transport_llp_eth_xeno_free(llp);

	firefly_event_queue_xeno_free(&event_queue);

	ping_pass_test(TEST_DONE);
	pingpong_test_print_results(ping_tests, PING_NBR_TESTS, "Ping");

	return 0;
}
