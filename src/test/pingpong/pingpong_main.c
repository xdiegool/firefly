#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <pthread.h>

#include "test/pingpong/pong_pudp.h"
#include "test/pingpong/ping_pudp.h"

// Our threads.
pthread_t pong_thread;
pthread_t ping_thread;

void start_pong()
{
	int err = pthread_create(&pong_thread, NULL, pong_main_thread, NULL);
	if (err == -1) {
		fprintf(stderr, "Could not create pong_main_thread thread.\n");
		exit(EXIT_FAILURE);
	}
}

void start_ping()
{
	int err = pthread_create(&ping_thread, NULL, ping_main_thread, NULL);
	if (err == -1) {
		fprintf(stderr, "Could not create ping_main_thread thread.\n");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	if (argc == 1) {
		start_pong();
		start_ping();
		pthread_join(ping_thread, NULL);
		pthread_join(pong_thread, NULL);
		return EXIT_SUCCESS;
	} else if (argc == 2 ) {
		if (strcmp(argv[1], "pong") == 0) {
			start_pong();
			pthread_join(pong_thread, NULL);
			return EXIT_SUCCESS;
		} else if (strcmp(argv[1], "ping") == 0) {
			start_ping();
			pthread_join(ping_thread, NULL);
			return EXIT_SUCCESS;
		} else {
			fprintf(stderr,
				"Bad paramter."
				" Valid names are \"pong\" and \"ping\".\n");
			return EXIT_FAILURE;
		}
	} else {
		fprintf(stderr,
			"Too many arguments given. None or one expected.\n");
		return EXIT_FAILURE;
	}
}
