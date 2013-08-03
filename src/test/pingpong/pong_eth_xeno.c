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

static RT_TASK *reader_thread = NULL;
static RT_TASK *main_thread = NULL;
static RT_TASK *event_thread = NULL;

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
static RT_SEM pong_done_signal;
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
void pong_connection_opened(struct firefly_connection *conn);

struct firefly_connection_actions conn_actions = {
	.channel_opened		= pong_chan_opened,
	.channel_closed		= pong_chan_closed,
	.channel_recv		= pong_chan_received,
	// New -v
	.channel_rejected	= NULL,
	.channel_restrict	= pong_chan_on_restrict_request,
	.channel_restrict_info	= pong_chan_on_restrict_change,
	.connection_opened = pong_connection_opened
};

void pong_chan_on_restrict_change(struct firefly_channel *chan,
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
		pong_pass_test(DATA_SEND);
	}
}

bool pong_chan_on_restrict_request(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	return true;
}

void pong_connection_opened(struct firefly_connection *conn)
{
	UNUSED_VAR(conn);
	pong_pass_test(CONNECTION_OPEN);
}

int64_t pong_connection_received(
		struct firefly_transport_llp *llp, char *mac_addr)
{
	/* If address is correct, open a connection. */
	if (strncmp(mac_addr, ping_mac_addr, strlen(ping_mac_addr)) == 0) {
		return firefly_connection_open(&conn_actions,
				firefly_transport_eth_xeno_memfuncs(), event_queue,
				firefly_transport_connection_eth_xeno_new(
				llp, mac_addr, pong_iface));
	} else {
		fprintf(stderr, "ERROR: Received unknown connection: %s\n",
				mac_addr);
		return 0;
	}
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
	UNUSED_VAR(chan);
	rt_sem_v(&pong_done_signal);
	pong_pass_test(CHAN_CLOSE);
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
	pong_pass_test(DATA_RECEIVE);
	struct firefly_channel *chan = ctx;
	pingpong_data pong_data = PONG_DATA;
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	labcomm_encode_pingpong_data(enc, &pong_data);
	pong_pass_test(DATA_SEND);
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
	if (reader_thread && rt_task_same(self, reader_thread)) printf("Reader thread\n");
	if (main_thread && rt_task_same(self, main_thread)) printf("Main thread\n");
	if (event_thread && rt_task_same(self, event_thread)) printf("Event thread\n");

    printf("\nSIGDEBUG received, reason %d: %s\n", reason,
       reason <= SIGDEBUG_WATCHDOG ? reason_str[reason] : "<unknown>");
    /* Dump a backtrace of the frame which caused the switch to
       secondary mode: */
    nentries = backtrace(bt,sizeof(bt) / sizeof(bt[0]));
    backtrace_symbols_fd(bt,nentries,fileno(stdout));
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
	RT_TASK reader_task;
	RT_TASK main_task;

	pong_iface = PONG_IFACE;
	ping_mac_addr = PING_MAC_ADDR;

	for (int i = 1; i < argc; i += 2) {
		if (strncmp(argv[i], "-i", 2) == 0) {
			pong_iface = argv[i+1];
		} else if (strncmp(argv[i], "-m", 2) == 0) {
			ping_mac_addr = argv[i+1];
		}
	}

	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = warn_upon_switch;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGDEBUG, &sa, NULL);

	printf("Hello, Firefly Ethernet from Pong!\n");

	mlockall(MCL_CURRENT | MCL_FUTURE);
	res = rt_task_shadow(&main_task, "main_task", MAIN_TASK_PRIO, 0);
	main_thread = &main_task;
	if (res) {
		fprintf(stderr, "ERROR: Failed shadow task, %s\n", strerror(-res));
	}

	rt_sem_create(&pong_done_signal, "ping_done_sem", 0, S_PRIO);

	pong_init_tests();

	event_queue = firefly_event_queue_xeno_new(EVENT_POOL_SIZE,
			EVENT_LOOP_PRIO, EVENT_TIMEOUT);
	res = firefly_event_queue_xeno_run(event_queue);
	if (res) {
		fprintf(stderr, "ERROR: starting event thread.\n");
	}
	event_thread = firefly_event_queue_xeno_loop(event_queue);

	struct firefly_transport_llp *llp =
			firefly_transport_llp_eth_xeno_new(pong_iface,
					pong_connection_received, event_queue);

	res = rt_mutex_create(&rtarg.lock, "reader_mutex");
	if (res) {
		fprintf(stderr, "ERROR: init mutex.\n");
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

	// Pong is started
	rt_sem_p(&pong_done_signal, TM_INFINITE);

	rt_mutex_acquire(&rtarg.lock, 1000000);
	rtarg.finish = true;
	rt_mutex_release(&rtarg.lock);
	rt_task_join(reader_thread);

	firefly_transport_llp_eth_xeno_free(llp);
	firefly_event_queue_xeno_free(&event_queue);

	pong_pass_test(TEST_DONE);
	pingpong_test_print_results(pong_tests, PONG_NBR_TESTS, "Pong");

	return 0;
}
