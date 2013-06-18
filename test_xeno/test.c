#define _POSIX_C_SOURCE (200112L)
#include <pthread.h>

// standard includes
#include <stdio.h>
#include <unistd.h>			// defines close
#include <stdbool.h>
#include <stdlib.h>

// Extra includes
#include <execinfo.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>

// system
#include <sys/time.h>
#include <sys/mman.h>
#ifdef __XENO__
#include <native/task.h>
#endif

// Network
#include <netinet/ether.h>	// defines ETH_P_ALL, AF_PACKET
#include <arpa/inet.h>		// defines htons
#include <linux/if.h>		// defines ifreq, IFNAMSIZ
#include <netpacket/packet.h>	// defines sockaddr_ll
#include <sys/ioctl.h>		// defines SIOCGIFINDEX
#include <sys/select.h>

static bool teardown = false;
static pthread_mutex_t exit_mutex;

static inline void timespec_diff(const struct timespec *f, const struct timespec *t,
		struct timespec *res)
{
	res->tv_sec = t->tv_sec - f->tv_sec;
	res->tv_nsec = t->tv_nsec - f->tv_nsec;
	long diff_sec = res->tv_nsec / 1000000000;
	res->tv_sec += diff_sec;
	res->tv_nsec -= (diff_sec) * 1000000000;

	if (res->tv_nsec < 0 && res->tv_sec > 0) {
		res->tv_sec--;
		res->tv_nsec += 1000000000;
	} else if (res->tv_nsec > 0 && res->tv_sec < 0) {
		res->tv_sec++;
		res->tv_nsec -= 1000000000;
	}
}

static inline void timespec_add(const struct timespec *add, struct timespec *res)
{
	res->tv_sec += add->tv_sec;
	res->tv_nsec += add->tv_nsec;
	if (res->tv_nsec > 1000000000) {
		res->tv_sec += res->tv_nsec / 1000000000;
		res->tv_nsec = res->tv_nsec % 1000000000;
	}
}

static inline void timespec_abs(struct timespec *t)
{
	if (t->tv_sec < 0) {
		t->tv_sec = -t->tv_sec;
	}
	if (t->tv_nsec < 0) {
		t->tv_nsec = -t->tv_nsec;
	}
}

static inline bool timespec_gt(const struct timespec *lhs, struct timespec *rhs)
{
	return rhs->tv_sec == lhs->tv_sec ?
		lhs->tv_nsec > rhs->tv_nsec : lhs->tv_sec > rhs->tv_sec;
}

static struct timespec acc_delay;
static struct timespec max_delay;
static struct timespec min_delay;
static long pkg_count = 0;
static struct timespec acc_diff;
static struct timespec max_diff;
static struct timespec min_diff;
// Count the difference from expected time

int receive_timespec(int soc, struct timespec *time_data,
		struct timespec *time_now, const struct timespec *timeout)
{
	int err;
	size_t pkg_len;
	struct sockaddr_ll addr;
	socklen_t addr_len;
	fd_set fs;
	FD_ZERO(&fs);
	FD_SET(soc, &fs);
	err = pselect(soc + 1, &fs, NULL, NULL, timeout, NULL);
	if (err == 0) {
		return -1;
	}
	if (err < 0) {
		fprintf(stderr, "Failed pselect %d\n", err);
		return -2;
	}
	err = clock_gettime(CLOCK_MONOTONIC, time_now);
	if (err < 0) {
		fprintf(stderr, "Could not get current time %d\n", err);
		return -2;
	}

	pkg_len = 0;
	err = ioctl(soc, FIONREAD, &pkg_len);
	if (err < 0) {
		fprintf(stderr, "Failed to get package size %d\n", err);
		return -2;
	}
	if (pkg_len != sizeof(struct timespec)) {
		fprintf(stderr, "Package size corrupt %zu\n", pkg_len);
		return -2;
	}
	err = recvfrom(soc, time_data, pkg_len, 0,
			(struct sockaddr *) &addr, &addr_len);
	if (err != pkg_len) {
		fprintf(stderr, "Could not read entire packet %d\n", err);
		return -2;
	}
	return 0;
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

struct thread_args {
	int socket;
	const struct timespec *timeout;
	const struct timespec *interval;
	const struct sockaddr_ll *addr;
};

void *thread_receiver_run(void *arg)
{
	int err;
#ifdef __XENO__
	err = pthread_set_mode_np(0, PTHREAD_WARNSW | PTHREAD_PRIMARY);
	if (err != 0) {
		printf("Error set mode %d\n", err);
		return NULL;
	}
#endif
	int soc;
	const struct timespec *timeout;
	const struct timespec *interval;
	struct timespec time_data;
	struct timespec time_now;
	struct timespec time_tmp;
	struct timespec expected_time;

	struct thread_args *recv_args = arg;
	soc = recv_args->socket;
	interval = recv_args->interval;
	timeout = recv_args->timeout;

	err = receive_timespec(soc, &time_data, &time_now, NULL);
	if (err < 0) {
		return NULL;
	}
	timespec_diff(&time_data, &time_now, &time_tmp);
	timespec_add(&time_tmp, &acc_delay);
	memcpy(&max_delay, &time_tmp, sizeof(time_tmp));
	memcpy(&min_delay, &time_tmp, sizeof(time_tmp));
	pkg_count++;
	memcpy(&expected_time, &time_now, sizeof(time_now));

	/*FILE *fp;*/
	/*fp = fopen("test.txt", "w");*/
	/*if (fp == NULL) {*/
		/*fprintf(stderr, "ERROR open file\n");*/
		/*return NULL;*/
	/*}*/
/*#ifdef __XENO__*/
	/*__real_fprintf(fp, "HELLO\n");*/
/*#else*/
	/*fprintf(fp, "HELLO\n");*/
/*#endif*/
	/*fclose(fp);*/
	while(!teardown) {
		err = receive_timespec(soc, &time_data, &time_now, timeout);
		if (err == -1) {
			if (teardown)
				break;
			else {
				fprintf(stderr, "Timed out\n");
				return NULL;
			}
		} else if (err < 0) {
			return NULL;
		}
		timespec_diff(&time_data, &time_now, &time_tmp);
		timespec_add(&time_tmp, &acc_delay);
		if (timespec_gt(&time_tmp, &max_delay)) {
			memcpy(&max_delay, &time_tmp, sizeof(time_tmp));
		} else if (timespec_gt(&min_delay, &time_tmp)) {
			memcpy(&min_delay, &time_tmp, sizeof(time_tmp));
		}
		timespec_add(interval, &expected_time);
		timespec_diff(&expected_time, &time_now, &time_tmp);
		timespec_abs(&time_tmp);
		timespec_add(&time_tmp, &acc_diff);
		if (timespec_gt(&time_tmp, &max_diff)) {
			memcpy(&max_diff, &time_tmp, sizeof(time_tmp));
		} else if (timespec_gt(&min_diff, &time_tmp)) {
			memcpy(&min_diff, &time_tmp, sizeof(time_tmp));
		}
		pkg_count++;
	}
	return NULL;
}

void *thread_sender_run(void *arg)
{
	int err;
#ifdef __XENO__
	err = pthread_set_mode_np(0, PTHREAD_WARNSW | PTHREAD_PRIMARY);
	if (err != 0) {
		printf("Error set mode %d\n", err);
		return NULL;
	}
#endif
	int soc;
	const struct timespec *interval;
	const struct sockaddr_ll *addr;

	struct thread_args *args = arg;
	soc = args->socket;
	interval = args->interval;
	addr = args->addr;

	pthread_condattr_t cond_attr;
	pthread_cond_t interval_cond;
	pthread_mutex_t interval_mutex;
	pthread_condattr_init(&cond_attr);
	err = pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
	if (err != 0) {
		fprintf(stderr, "Failed setting monotonic clock\n");
		return NULL;
	}
	pthread_cond_init(&interval_cond, &cond_attr);
	if (err != 0) {
		fprintf(stderr, "Failed cond init.\n");
		return NULL;
	}
	pthread_mutex_init(&interval_mutex, NULL);
	if (err != 0) {
		fprintf(stderr, "Failed mutex init.\n");
		return NULL;
	}
	struct timespec next_send;
	struct timespec time_now;
	struct timespec time_wait;
	clock_gettime(CLOCK_MONOTONIC, &next_send);
	pthread_mutex_lock(&exit_mutex);
	while(!teardown) {
		pthread_mutex_unlock(&exit_mutex);
		timespec_add(interval, &next_send);
		pthread_mutex_lock(&interval_mutex);
		err = pthread_cond_timedwait(&interval_cond, &interval_mutex,
				&next_send);
		pthread_mutex_unlock(&interval_mutex);
		if (err != 0  && err != ETIMEDOUT && !teardown) {
			fprintf(stderr, "Failed to sleep. %d\n", err);
			teardown = true;
			continue;
		}
		clock_gettime(CLOCK_MONOTONIC, &time_now);
		err = sendto(soc, &time_now, sizeof(time_now), 0,
				(struct sockaddr *)addr, sizeof(*addr));
		if (err < 0) {
			close(soc);
			fprintf(stderr, "Failed to send data.\n");
			return NULL;
		}
		pthread_mutex_lock(&exit_mutex);
	}
	pthread_mutex_unlock(&exit_mutex);
	return NULL;
}

static void sig_h(int sig)
{
	pthread_mutex_lock(&exit_mutex);
	teardown = true;
	pthread_mutex_unlock(&exit_mutex);
}

int main(int argc, char **argv)
{
	int err;
	struct ifreq ifr;
	struct sockaddr_ll addr;
	struct sockaddr_ll remote_addr;
	struct timespec interval;
	struct timespec timeout;
	char *iface_name;
	char *mac_address;
	int soc;
	pthread_t receiver_thread;
	pthread_t sender_thread;
	pthread_attr_t thread_attr;
	struct sched_param thread_prio;
	char runserv = 'b';

	if (argc < 4) {
		fprintf(stderr, "Usage: test <interface> <mac_addr> <interval(ns)> [send|recv]\n");
		return 1;
	}

	mlockall(MCL_CURRENT | MCL_FUTURE);

#ifdef __XENO__
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = warn_upon_switch;
	sa.sa_flags = SA_SIGINFO;
	/*sigaction(SIGDEBUG, &sa, NULL);*/
	sigaction(SIGXCPU, &sa, NULL);
#endif

	pthread_attr_init(&thread_attr);
	pthread_attr_setschedpolicy(&thread_attr, SCHED_FIFO);

	iface_name = argv[1];
	mac_address = argv[2];
	interval.tv_sec = 0;
	interval.tv_nsec = atol(argv[3]);
	if (interval.tv_nsec >= 1000000000) {
		interval.tv_sec = interval.tv_nsec / 1000000000;
		interval.tv_nsec = interval.tv_nsec % 1000000000;
	}
	timeout.tv_sec = 0;
	timeout.tv_nsec = 2 * interval.tv_nsec;
	if (timeout.tv_nsec >= 1000000000) {
		timeout.tv_sec = timeout.tv_nsec / 1000000000;
		timeout.tv_nsec = timeout.tv_nsec % 1000000000;
	}

	if (argc >= 5) {
		runserv = strncmp("recv", argv[4], 4) == 0 ? 'r' : 
			(strncmp("send", argv[4], 4) == 0 ? 's' : 'b');
	}

	pthread_mutex_init(&exit_mutex, NULL);
	if (signal(SIGINT, sig_h) == SIG_ERR) {
		fprintf(stderr, "could not set signal handler.\n");
		return 1;
	}

	err = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL));
	if(err < 0){
		fprintf(stderr, "Failed to open socket!\n");
		return 2;
	}
	soc = err;
	/* Set the interface name */
	strncpy(ifr.ifr_name, iface_name, IFNAMSIZ);
	err = ioctl(soc, SIOCGIFINDEX, &ifr);
	if(err < 0){
		close(soc);
		fprintf(stderr, "Failed to set interface!\n");
		return 3;
	}

	/* init mem of local_addr with zeros */
	memset(&addr, 0, sizeof(struct sockaddr_ll));
	addr.sll_family   = AF_PACKET;
	addr.sll_protocol = htons(0x1337);
	addr.sll_ifindex  = ifr.ifr_ifindex;
	/* Bind socket to specified interface */
	err = bind(soc, (struct sockaddr *)&addr, sizeof(addr));
	if(err < 0){
		close(soc);
		fprintf(stderr, "Failed to bind socket.\n");
		return 4;
	}

	memset(&remote_addr, 0, sizeof(struct sockaddr_ll));
	remote_addr.sll_family   = AF_PACKET;
	remote_addr.sll_protocol = htons(0x1337);
	remote_addr.sll_ifindex  = ifr.ifr_ifindex;
	ether_aton_r(mac_address, (void *)&remote_addr.sll_addr);
	remote_addr.sll_halen    = 6;

	struct thread_args args = {
		.socket = soc,
		.timeout = &timeout,
		.interval = &interval,
		.addr = &remote_addr
	};

	if (runserv == 'b' || runserv == 'r') {
		printf("Staring receiver\n");
		thread_prio.sched_priority = 51;
		pthread_attr_setschedparam(&thread_attr, &thread_prio);
		err = pthread_create(&receiver_thread, &thread_attr, thread_receiver_run, &args);
		if (err < 0) {
			fprintf(stderr, "Failed to create receiver thread.\n");
			return 4;
		}
	}
	if (runserv == 'b' || runserv == 's') {
		printf("Staring sender\n");
		thread_prio.sched_priority = 50;
		pthread_attr_setschedparam(&thread_attr, &thread_prio);
		err = pthread_create(&sender_thread, &thread_attr, thread_sender_run, &args);
		if (err < 0) {
			fprintf(stderr, "Failed to create receiver thread.\n");
			return 4;
		}
	}

	if (runserv == 'b' || runserv == 's') {
		pthread_join(sender_thread, NULL);
	}
	if (runserv == 'b' || runserv == 'r') {
		pthread_join(receiver_thread, NULL);
	}
	if (runserv == 's') {
		return 0;
	}
	printf("Network data statistics:\n");
	printf("Number of packets received:\t%ld\n", pkg_count);
	printf("Meassured\tMAX\t\tMIN\t\tAVG\n");
	printf("TIMING:\t\t%ld.%09ld\t%ld.%09ld\t%ld.%09ld\n",
			max_diff.tv_sec, max_diff.tv_nsec,
			min_diff.tv_sec, min_diff.tv_nsec,
			(acc_diff.tv_sec / pkg_count),
			(acc_diff.tv_nsec / pkg_count));
	printf("DELAY:\t\t%ld.%09ld\t%ld.%09ld\t%ld.%09ld\n",
			max_delay.tv_sec, max_delay.tv_nsec,
			min_delay.tv_sec, min_delay.tv_nsec,
			(acc_delay.tv_sec / pkg_count),
			(acc_delay.tv_nsec / pkg_count));

	return 0;
}
