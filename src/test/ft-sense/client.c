#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>

#include <labcomm.h>
#include <protocol/firefly_protocol.h>
#include <transport/firefly_transport_eth_posix.h>
#include <utils/firefly_event_queue.h>
#include <gen/ft_sample.h>

#include "utils/cppmacros.h"

// Event thread stuff.
static bool event_added = false;
struct firefly_event_queue *event_queue;
void *event_thread(void *context);
int ff_event_adder(struct firefly_event_queue *eq, struct firefly_event *ev);
void *reader_thread_main(void *args);

static pthread_mutex_t event_run_lock = PTHREAD_MUTEX_INITIALIZER;
static bool event_run = true;

/*************************************/
/********** <CALLBACK HELL> **********/
/*************************************/
static pthread_mutex_t signal_lock = PTHREAD_MUTEX_INITIALIZER;
static bool signal_caught = false;
static void sig_h(int sig)
{
	printf("Caught signal: %d\n", sig);
	pthread_mutex_lock(&signal_lock);
	signal_caught = true;
	pthread_mutex_unlock(&signal_lock);
}

static unsigned long long int num_samp = 0;
void handle_sample_data(ft_sample_data *data, void *ctx) {
	UNUSED_VAR(ctx);
	printf("%c%c%c%c%c%d\n", data->a[0], data->a[1], data->a[2], data->a[3],
			data->a[4], data->a[5]);
	num_samp++;
}

void chan_opened(struct firefly_channel *chan)
{
	struct labcomm_encoder *enc = firefly_protocol_get_output_stream(chan);
	struct labcomm_decoder *dec = firefly_protocol_get_input_stream(chan);

	labcomm_decoder_register_ft_sample_data(dec, handle_sample_data, chan);
	labcomm_encoder_register_ft_sample_data(enc);
}

void chan_closed(struct firefly_channel *chan)
{
	firefly_transport_connection_eth_posix_close(
			firefly_channel_get_connection(chan));
}

bool chan_received(struct firefly_channel *chan)
{
	UNUSED_VAR(chan);
	printf("Channel recieved!\n");
	return true;
}

void channel_rejected(struct firefly_connection *conn)
{
	UNUSED_VAR(conn);
	fprintf(stderr, "ERROR: Channel was rejected.\n");
}

struct firefly_connection *connection_received(struct firefly_transport_llp *llp,
		char *mac_addr)
{
	return firefly_transport_connection_eth_posix_open(chan_opened,
			chan_closed, chan_received, event_queue, mac_addr, "em1", llp);
}

/**************************************/
/********** </CALLBACK HELL> **********/
/**************************************/

int main(void)
{
	uid_t uid;
	struct timespec begin, end;
	double exec_time;
	pthread_t event_thread_t;
	struct firefly_transport_llp *llp;
	struct firefly_connection *conn;
	int res;

	// Check that we're root or we can't use raw Ethernet sockets.
	uid = geteuid();
	if (uid != 0) {
		fprintf(stderr, "Need root to run these tests\n");
		return 1;
	}

	clock_gettime(CLOCK_MONOTONIC, &begin); // Find out why this doesn't work...

	// Create new event queue.
	event_queue = firefly_event_queue_new(&ff_event_adder, NULL);
	if (!event_queue) {
		fprintf(stderr, "Failed to create event queue.\n");
		return 1;
	}

	// Create event thread.
	res = pthread_create(&event_thread_t, NULL, event_thread, NULL);
	if (res) {
		fprintf(stderr, "ERROR: starting event thread.\n");
	}

	// Create new llp.
	llp = firefly_transport_llp_eth_posix_new("em1", connection_received);
	if (!llp) {
		fprintf(stderr, "Failed to create llp.\n");
		firefly_event_queue_free(&event_queue);
		return 1;
	}

	conn = firefly_transport_connection_eth_posix_open(chan_opened,
			chan_closed, chan_received, event_queue,
			"00:42:42:42:42:10", "em1", llp);

	firefly_channel_open(conn, channel_rejected);

	reader_thread_main(llp);

	pthread_mutex_lock(&event_run_lock);
	event_run = false;
	pthread_mutex_unlock(&event_run_lock);

	/*pthread_join(event_thread_t, NULL);*/

	clock_gettime(CLOCK_REALTIME, &end);
	exec_time = (double)(end.tv_sec - begin.tv_sec + (end.tv_nsec - begin.tv_nsec)/1e9);
	printf("\n\nRESULT:\n");
	printf("\tExecuted for %lf seconds.\n", exec_time);
	printf("\tGot a total of %llu samples.\n", num_samp);
	printf("\tAveraging %lf samples/second.\n",
			((double) num_samp)/exec_time);
	return 0;
}

/***********************************/
/********** READER THREAD **********/
/***********************************/
void *reader_thread_main(void *args)
{
	struct firefly_transport_llp *llp =
		(struct firefly_transport_llp *) args;

	bool run = true;
	while (run) {
		/*printf("Waiting for data...");*/
		firefly_transport_eth_posix_read(llp);
		/*printf(" Got it!\n");*/
		pthread_mutex_lock(&signal_lock);
		run = !signal_caught;
		pthread_mutex_unlock(&signal_lock);
	}

}

/*********************************/
/********** EVENT STUFF **********/
/*********************************/
static pthread_mutex_t event_queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t event_added_cond  = PTHREAD_COND_INITIALIZER;

void *event_thread(void *args)
{
	UNUSED_VAR(args);

	bool run = true;
	struct firefly_event *ev;
	while(run) {
		/*printf("Waiting for new event...\n");*/
		pthread_mutex_lock(&event_queue_lock);
		// Wait for signal that an event was added.
		while (firefly_event_queue_length(event_queue) < 1) {
			pthread_cond_wait(&event_added_cond,
					&event_queue_lock);
		}
		ev = firefly_event_pop(event_queue);
		pthread_mutex_unlock(&event_queue_lock);
		/*printf("Got one!\n");*/

		if (ev != NULL) {
			/*printf("Executing event...\n");*/
			firefly_event_execute(ev);
			/*printf("OK!\n");*/
		}

		pthread_mutex_lock(&event_run_lock);
		run = event_run;
		pthread_mutex_unlock(&event_run_lock);
	}
}

int ff_event_adder(struct firefly_event_queue *eq, struct firefly_event *ev)
{
	int ret;
	pthread_mutex_lock(&event_queue_lock);
	ret = firefly_event_add(eq, ev);
	// Signal that an event was added.
	event_added = true;
	pthread_cond_signal(&event_added_cond);
	pthread_mutex_unlock(&event_queue_lock);

	return ret;
}
